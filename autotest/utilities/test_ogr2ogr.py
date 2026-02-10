#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogr2ogr testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import shutil

import gdaltest
import ogrtest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_ogr2ogr_path() is None, reason="ogr2ogr not available"
)


@pytest.fixture()
def ogr2ogr_path():
    return test_cli_utilities.get_ogr2ogr_path()


###############################################################################
# Simple test


def test_ogr2ogr_1(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    _, err = gdaltest.runexternal_out_and_err(
        ogr2ogr_path + f" {output_shp} ../ogr/data/poly.shp"
    )
    assert err is None or err == "", "got error/warning"

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10

    feat0 = ds.GetLayer(0).GetFeature(0)
    assert (
        feat0.GetFieldAsDouble("AREA") == 215229.266
    ), "Did not get expected value for field AREA"
    assert (
        feat0.GetFieldAsString("PRFEDEA") == "35043411"
    ), "Did not get expected value for field PRFEDEA"


###############################################################################
# Test -sql


def test_ogr2ogr_2(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        ogr2ogr_path + f' {output_shp} ../ogr/data/poly.shp -sql "select * from poly"'
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10


###############################################################################
# Test -spat


def test_ogr2ogr_3(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        ogr2ogr_path
        + f" {output_shp} ../ogr/data/poly.shp -spat 479609 4764629 479764 4764817"
    )

    ds = ogr.Open(output_shp)
    if ogrtest.have_geos():
        assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4
    else:
        assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 5


###############################################################################
# Test -where


def test_ogr2ogr_4(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        ogr2ogr_path + f' {output_shp} ../ogr/data/poly.shp -where "EAS_ID=171"'
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1


###############################################################################
# Test -append


def test_ogr2ogr_5(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(ogr2ogr_path + f" {output_shp} ../ogr/data/poly.shp")
    # All 3 variants below should be equivalent
    gdaltest.runexternal(
        ogr2ogr_path + f" -update -append {output_shp} ../ogr/data/poly.shp"
    )
    gdaltest.runexternal(ogr2ogr_path + f" -append {output_shp} ../ogr/data/poly.shp")
    gdaltest.runexternal(
        ogr2ogr_path + f" -append -update {output_shp} ../ogr/data/poly.shp"
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 40

    feat10 = ds.GetLayer(0).GetFeature(10)
    assert (
        feat10.GetFieldAsDouble("AREA") == 215229.266
    ), "Did not get expected value for field AREA"
    assert (
        feat10.GetFieldAsString("PRFEDEA") == "35043411"
    ), "Did not get expected value for field PRFEDEA"


###############################################################################
# Test -overwrite


def test_ogr2ogr_6(ogr2ogr_path, pg_ds):

    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        f"{ogr2ogr_path} -f PostgreSQL '{pg_ds.GetDescription()}' ../ogr/data/poly.shp -nln tpoly"
    )
    assert "ERROR" not in ret

    ret = gdaltest.runexternal(
        f"{ogr2ogr_path} -f PostgreSQL '{pg_ds.GetDescription()}' ../ogr/data/poly.shp -nln tpoly -overwrite"
    )
    assert "ERROR" not in ret

    ds = ogr.Open(pg_ds.GetDescription())
    assert ds is not None
    assert ds.GetLayerByName("tpoly").GetFeatureCount() == 10


###############################################################################
# Test -gt


def test_ogr2ogr_7(ogr2ogr_path, pg_ds):

    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        f"{ogr2ogr_path} -f PostgreSQL '{pg_ds.GetDescription()}' ../ogr/data/poly.shp -nln tpoly -gt 1"
    )
    assert "ERROR" not in ret

    ds = ogr.Open(pg_ds.GetDescription())
    assert ds is not None
    assert ds.GetLayerByName("tpoly").GetFeatureCount() == 10


###############################################################################
# Test -t_srs


def test_ogr2ogr_8(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        ogr2ogr_path + f" -t_srs EPSG:4326 {output_shp} ../ogr/data/poly.shp"
    )

    ds = ogr.Open(output_shp)
    assert str(ds.GetLayer(0).GetSpatialRef()).find("1984") != -1


###############################################################################
# Test -a_srs


def test_ogr2ogr_9(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        ogr2ogr_path + f" -a_srs EPSG:4326 {output_shp} ../ogr/data/poly.shp"
    )

    ds = ogr.Open(output_shp)
    assert str(ds.GetLayer(0).GetSpatialRef()).find("1984") != -1


###############################################################################
# Test -select


def test_ogr2ogr_10(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    # Voluntary don't use the exact case of the source field names (#4502)
    gdaltest.runexternal(
        ogr2ogr_path + f" -select eas_id,prfedea {output_shp} ../ogr/data/poly.shp"
    )

    ds = ogr.Open(output_shp)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsDouble("EAS_ID") == 168
    assert feat.GetFieldAsString("PRFEDEA") == "35043411"
    feat = None
    ds = None


###############################################################################
# Test -lco


def test_ogr2ogr_11(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        ogr2ogr_path + f" -lco SHPT=POLYGONZ {output_shp} ../ogr/data/poly.shp"
    )

    ds = ogr.Open(output_shp)
    assert ds.GetLayer(0).GetLayerDefn().GetGeomType() == ogr.wkbPolygon25D


###############################################################################
# Test -nlt


def test_ogr2ogr_12(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        ogr2ogr_path + f" -nlt POLYGON25D {output_shp} ../ogr/data/poly.shp"
    )

    ds = ogr.Open(output_shp)
    assert ds.GetLayer(0).GetLayerDefn().GetGeomType() == ogr.wkbPolygon25D


###############################################################################
# Add explicit source layer name


def test_ogr2ogr_13(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(ogr2ogr_path + f" {output_shp} ../ogr/data/poly.shp poly")

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10


###############################################################################
# Test -segmentize


def test_ogr2ogr_14(ogr2ogr_path, tmp_path):

    output_shp = tmp_path / "poly.shp"

    # invalid value
    _, err = gdaltest.runexternal_out_and_err(
        ogr2ogr_path + f" -segmentize small_bits {output_shp} ../ogr/data/poly.shp poly"
    )
    assert "Failed to parse" in err
    assert not output_shp.exists()

    _, err = gdaltest.runexternal_out_and_err(
        ogr2ogr_path + f" -segmentize 100 {output_shp} ../ogr/data/poly.shp poly"
    )

    assert not err

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    feat = ds.GetLayer(0).GetNextFeature()
    assert feat.GetGeometryRef().GetGeometryRef(0).GetPointCount() == 36


###############################################################################
# Test -overwrite with a shapefile


def test_ogr2ogr_15(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(ogr2ogr_path + f" {output_shp} ../ogr/data/poly.shp")

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ds = None

    # Overwrite
    gdaltest.runexternal(ogr2ogr_path + f" -overwrite {tmp_path} ../ogr/data/poly.shp")

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10


###############################################################################
# Test -fid


def test_ogr2ogr_16(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(ogr2ogr_path + f" -fid 8 {output_shp} ../ogr/data/poly.shp")

    src_ds = ogr.Open("../ogr/data/poly.shp")
    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 1
    src_feat = src_ds.GetLayer(0).GetFeature(8)
    feat = ds.GetLayer(0).GetNextFeature()
    assert feat.GetField("EAS_ID") == src_feat.GetField("EAS_ID")


###############################################################################
# Test -progress


def test_ogr2ogr_17(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    ret = gdaltest.runexternal(
        ogr2ogr_path + f" -progress {output_shp} ../ogr/data/poly.shp"
    )
    assert (
        ret.find("0...10...20...30...40...50...60...70...80...90...100 - done.") != -1
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10


###############################################################################
# Test -wrapdateline


@pytest.mark.require_geos
def test_ogr2ogr_18(ogr2ogr_path, tmp_path):

    src_shp = str(tmp_path / "wrapdateline_src.shp")
    dst_shp = str(tmp_path / "wrapdateline_dst.shp")

    with ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(src_shp) as ds:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32660)
        lyr = ds.CreateLayer("wrapdateline_src", srs=srs)
        feat = ogr.Feature(lyr.GetLayerDefn())
        geom = ogr.CreateGeometryFromWkt(
            "POLYGON((700000 4000000,800000 4000000,800000 3000000,700000 3000000,700000 4000000))"
        )
        feat.SetGeometryDirectly(geom)
        lyr.CreateFeature(feat)

    gdaltest.runexternal(
        ogr2ogr_path + f" -wrapdateline -t_srs EPSG:4326 {dst_shp} {src_shp}"
    )

    expected_wkt = "MULTIPOLYGON (((179.222391385437 36.124095832137,180.0 36.1071354434926,180.0 36.107135443432,180.0 27.0904291237556,179.017505655194 27.1079795236266,179.222391385437 36.124095832137)),((-180 36.1071354434425,-179.667822828784 36.0983491954849,-179.974688335432 27.0898861430914,-180 27.0904291237129,-180 27.090429123727,-180 36.107135443432,-180 36.1071354434425)))"
    expected_wkt2 = "MULTIPOLYGON (((179.017505655194 27.1079795236266,179.222391385437 36.124095832137,180.0 36.1071354434926,180.0 36.107135443432,180.0 27.0904291237556,179.017505655194 27.1079795236266)),((-180 27.090429123727,-180 36.107135443432,-180 36.1071354434425,-179.667822828784 36.0983491954849,-179.974688335432 27.0898861430914,-180 27.0904291237129,-180 27.090429123727)))"  # with geos OverlayNG
    expected_wkt3 = "MULTIPOLYGON (((180.0 36.1071354434926,180.0 36.107135443432,180.0 27.0904291237556,179.017505655194 27.1079795236266,179.222391385437 36.124095832137,180.0 36.1071354434926)),((-179.667822828784 36.0983491954849,-179.974688335432 27.0898861430914,-180 27.0904291237129,-180 27.090429123727,-180 36.107135443432,-180 36.1071354434425,-179.667822828784 36.0983491954849)))"

    ds = ogr.Open(dst_shp)
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    try:
        ogrtest.check_feature_geometry(feat, expected_wkt)
    except AssertionError:
        try:
            ogrtest.check_feature_geometry(feat, expected_wkt2)
        except AssertionError:
            ogrtest.check_feature_geometry(feat, expected_wkt3)


###############################################################################
# Test automatic polygon splitting, and also antimeridian being intersected
# at line of constant easting.


@pytest.mark.require_geos
def test_ogr2ogr_polygon_splitting(ogr2ogr_path, tmp_path):

    src_shp = str(tmp_path / "wrapdateline_src.shp")
    dst_shp = str(tmp_path / "wrapdateline_dst.shp")

    with ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(src_shp) as ds:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32601)
        lyr = ds.CreateLayer("wrapdateline_src", srs=srs)
        feat = ogr.Feature(lyr.GetLayerDefn())
        geom = ogr.CreateGeometryFromWkt(
            "POLYGON((377120 7577600,418080 7577600,418080 7618560,377120 7618560,377120 7577600))"
        )
        feat.SetGeometryDirectly(geom)
        lyr.CreateFeature(feat)

    gdaltest.runexternal(f"{ogr2ogr_path} -t_srs EPSG:4326 {dst_shp} {src_shp}")

    ds = ogr.Open(dst_shp)
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.GetGeometryType() == ogr.wkbMultiPolygon
    assert geom.GetGeometryCount() == 2


###############################################################################
# Test -clipsrc


@pytest.mark.require_geos
def test_ogr2ogr_19(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        f"{ogr2ogr_path} {output_shp} ../ogr/data/poly.shp -clipsrc spat_extent -spat 479609 4764629 479764 4764817"
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (
        479609,
        479764,
        4764629,
        4764817,
    ), "unexpected extent"


###############################################################################
# Test correct remap of fields when laundering to Shapefile format
# Test that the data is going into the right field
# FIXME: Any field is skipped if a subsequent field with same name is found.


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_20(ogr2ogr_path, tmp_path):

    expected_fields = [
        "a",
        "A_1",
        "a_1_2",
        "aaaaaAAAAA",
        "aAaaaAAA_1",
        "aaaaaAAAAB",
        "aaaaaAAA_2",
        "aaaaaAAA_3",
        "aaaaaAAA_4",
        "aaaaaAAA_5",
        "aaaaaAAA_6",
        "aaaaaAAA_7",
        "aaaaaAAA_8",
        "aaaaaAAA_9",
        "aaaaaAAA10",
    ]
    expected_data = [
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
        "10",
        "11",
        "12",
        "13",
        "14",
        "15",
    ]

    gdaltest.runexternal(ogr2ogr_path + f" {tmp_path} data/Fields.csv")

    ds = ogr.Open(f"{tmp_path}/Fields.dbf")

    assert ds is not None
    layer_defn = ds.GetLayer(0).GetLayerDefn()
    assert layer_defn.GetFieldCount() == 15, "Unexpected field count"

    feat = ds.GetLayer(0).GetNextFeature()
    for i in range(layer_defn.GetFieldCount()):
        assert layer_defn.GetFieldDefn(i).GetNameRef() == expected_fields[i]
        assert feat.GetFieldAsString(i) == expected_data[i]


###############################################################################
# Test ogr2ogr when the output driver has already created the fields
# at dataset creation (#3247)


@pytest.mark.require_driver("GPX")
@pytest.mark.require_driver("CSV")
def test_ogr2ogr_21(ogr2ogr_path, tmp_path):

    output_gpx = str(tmp_path / "testogr2ogr21.gpx")

    gdaltest.runexternal(
        ogr2ogr_path
        + f" -f GPX {output_gpx} data/dataforogr2ogr21.csv "
        + '-sql "SELECT name AS route_name, 0 as route_fid FROM dataforogr2ogr21" -nlt POINT -nln route_points'
    )
    assert "<name>NAME</name>" in open(output_gpx, "rt").read()


###############################################################################
# Test ogr2ogr when the output driver delays the destination layer defn creation (#3384)


@pytest.mark.require_driver("MapInfo File")
@pytest.mark.require_driver("CSV")
def test_ogr2ogr_22(ogr2ogr_path, tmp_path):

    output_mif = str(tmp_path / "testogr2ogr22.mif")

    gdaltest.runexternal(
        ogr2ogr_path
        + f' -f "MapInfo File" {output_mif} data/dataforogr2ogr21.csv '
        + '-sql "SELECT comment, name FROM dataforogr2ogr21" -nlt POINT'
    )
    ds = ogr.Open(output_mif)

    assert ds is not None
    ds.GetLayer(0).GetLayerDefn()
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString("name") == "NAME"
    assert feat.GetFieldAsString("comment") == "COMMENT"


###############################################################################
# Same as previous but with -select


@pytest.mark.require_driver("MapInfo File")
@pytest.mark.require_driver("CSV")
def test_ogr2ogr_23(ogr2ogr_path, tmp_path):

    output_mif = str(tmp_path / "testogr2ogr23.mif")

    gdaltest.runexternal(
        ogr2ogr_path
        + f' -f "MapInfo File" {output_mif} data/dataforogr2ogr21.csv '
        + '-sql "SELECT comment, name FROM dataforogr2ogr21" -select comment,name -nlt POINT'
    )
    ds = ogr.Open(output_mif)

    assert ds is not None
    ds.GetLayer(0).GetLayerDefn()
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString("name") == "NAME"
    assert feat.GetFieldAsString("comment") == "COMMENT"


###############################################################################
# Test -clipsrc with WKT geometry (#3530)


@pytest.mark.require_geos
def test_ogr2ogr_24(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        ogr2ogr_path
        + f' {output_shp} ../ogr/data/poly.shp -clipsrc "POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"'
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (
        479609,
        479764,
        4764629,
        4764817,
    ), "unexpected extent"


###############################################################################
# Test -clipsrc with clip from external datasource


@pytest.mark.require_driver("CSV")
@pytest.mark.require_geos
def test_ogr2ogr_25(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")
    clip_csv = str(tmp_path / "clip.csv")

    f = open(clip_csv, "wt")
    f.write("foo,WKT\n")
    f.write(
        'foo,"POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"\n'
    )
    f.close()

    gdaltest.runexternal(
        f"{ogr2ogr_path} {output_shp} ../ogr/data/poly.shp -clipsrc {clip_csv} -clipsrcwhere foo='foo'"
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (
        479609,
        479764,
        4764629,
        4764817,
    ), "unexpected extent"


###############################################################################
# Test -clipdst with WKT geometry (#3530)


@pytest.mark.require_geos
def test_ogr2ogr_26(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        f'{ogr2ogr_path} {output_shp} ../ogr/data/poly.shp -clipdst "POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"'
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (
        479609,
        479764,
        4764629,
        4764817,
    ), "unexpected extent"


###############################################################################
# Test -clipdst with clip from external datasource


@pytest.mark.require_driver("CSV")
@pytest.mark.require_geos
def test_ogr2ogr_27(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")
    clip_csv = str(tmp_path / "clip.csv")

    f = open(clip_csv, "wt")
    f.write("foo,WKT\n")
    f.write(
        'foo,"POLYGON((479609 4764629,479609 4764817,479764 4764817,479764 4764629,479609 4764629))"\n'
    )
    f.close()

    gdaltest.runexternal(
        f'{ogr2ogr_path} -nlt MULTIPOLYGON {output_shp} ../ogr/data/poly.shp -clipdst {clip_csv} -clipdstsql "SELECT * from clip"'
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 4

    assert ds.GetLayer(0).GetExtent() == (
        479609,
        479764,
        4764629,
        4764817,
    ), "unexpected extent"


###############################################################################
# Test -clipdst with clip from bounding box


@pytest.mark.require_geos
def test_ogr2ogr_clipdst_bbox(ogr2ogr_path, tmp_path):

    output_shp = tmp_path / "poly.shp"

    xmin = 479400
    xmax = 480300
    ymin = 4764500
    ymax = 4765100

    _, err = gdaltest.runexternal_out_and_err(
        f"{ogr2ogr_path} {output_shp} ../ogr/data/poly.shp -clipdst {xmin}x {ymin} {xmax} {ymax}"
    )

    assert "cannot load dest clip geometry" in err
    assert not output_shp.exists()

    _, err = gdaltest.runexternal_out_and_err(
        f"{ogr2ogr_path} {output_shp} ../ogr/data/poly.shp -clipdst {xmin} {ymin} {xmax} {ymax}"
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 7

    assert ds.GetLayer(0).GetExtent() == (
        xmin,
        xmax,
        ymin,
        ymax,
    ), "unexpected extent"


###############################################################################
# Test -wrapdateline on linestrings


def test_ogr2ogr_28(ogr2ogr_path, tmp_path):

    src_shp = str(tmp_path / "wrapdateline_src.shp")
    dst_shp = str(tmp_path / "wrapdateline_dst.shp")

    with ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(src_shp) as ds:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        lyr = ds.CreateLayer("wrapdateline_src", srs=srs)
        feat = ogr.Feature(lyr.GetLayerDefn())
        geom = ogr.CreateGeometryFromWkt(
            "LINESTRING(160 0,165 1,170 2,175 3,177 4,-177 5,-175 6,-170 7,-177 8,177 9,170 10)"
        )
        feat.SetGeometryDirectly(geom)
        lyr.CreateFeature(feat)

    gdaltest.runexternal(f"{ogr2ogr_path} -wrapdateline {dst_shp} {src_shp}")

    expected_wkt = "MULTILINESTRING ((160 0,165 1,170 2,175 3,177 4,180 4.5),(-180 4.5,-177 5,-175 6,-170 7,-177 8,-180 8.5),(180 8.5,177 9,170 10))"
    expected_geom = ogr.CreateGeometryFromWkt(expected_wkt)
    ds = ogr.Open(dst_shp)
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(feat, expected_geom)


###############################################################################
# Test -wrapdateline on polygons


@pytest.mark.require_geos
@pytest.mark.parametrize("i", (0, 1))
def test_ogr2ogr_29(ogr2ogr_path, tmp_path, i):

    src_shp = str(tmp_path / "wrapdateline_src.shp")
    dst_shp = str(tmp_path / "wrapdateline_dst.shp")

    with ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(src_shp) as ds:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        lyr = ds.CreateLayer("wrapdateline_src", srs=srs)
        feat = ogr.Feature(lyr.GetLayerDefn())

        if i == 0:
            geom = ogr.CreateGeometryFromWkt(
                "POLYGON((179 40,179.5 40,-179.5 40,-179 40,-170 40,-165 40,-165 30,-170 30,-179 30,-179.5 30,179.5 30,179 30,179 40))"
            )
        else:
            geom = ogr.CreateGeometryFromWkt(
                "POLYGON((-165 30,-170 30,-179 30,-179.5 30,179.5 30,179 30,179 40,179.5 40,-179.5 40,-179 40,-170 40,-165 40,-165 30))"
            )
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)

    gdaltest.runexternal(f"{ogr2ogr_path} -wrapdateline {dst_shp} {src_shp}")

    expected_wkt = "MULTIPOLYGON (((180 30,179.5 30.0,179 30,179 40,179.5 40.0,180 40,180 30)),((-180 40,-179.5 40.0,-179 40,-170 40,-165 40,-165 30,-170 30,-179 30,-179.5 30.0,-180 30,-180 40)))"
    expected_geom = ogr.CreateGeometryFromWkt(expected_wkt)
    ds = ogr.Open(dst_shp)
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(feat, expected_geom)


###############################################################################
# Test -splitlistfields option


@pytest.mark.require_driver("GML")
def test_ogr2ogr_30(ogr2ogr_path, tmp_path):

    src_gml = str(tmp_path / "testlistfields.gml")
    dst_dbf = str(tmp_path / "test_ogr2ogr_30.dbf")

    shutil.copy("../ogr/data/gml/testlistfields.gml", src_gml)

    gdaltest.runexternal(f"{ogr2ogr_path} -splitlistfields {dst_dbf} {src_gml}")

    ds = ogr.Open(dst_dbf)
    assert ds is not None
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if (
        feat.GetField("attrib11") != "value1"
        or feat.GetField("attrib12") != "value2"
        or feat.GetField("attrib2") != "value3"
        or feat.GetField("attrib31") != 4
        or feat.GetField("attrib32") != 5
        or feat.GetField("attrib41") != 6.1
        or feat.GetField("attrib42") != 7.1
    ):
        feat.DumpReadable()
        pytest.fail("did not get expected attribs")

    ds = None


###############################################################################
# Test that -overwrite works if the output file doesn't yet exist (#3825)


def test_ogr2ogr_31(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "poly.shp")

    gdaltest.runexternal(
        ogr2ogr_path + f" -overwrite {output_shp} ../ogr/data/poly.shp"
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 10


###############################################################################
# Test that -append/-overwrite to a single-file shapefile work without specifying -nln


def test_ogr2ogr_32(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "test_ogr2ogr_32.shp")

    gdaltest.runexternal(ogr2ogr_path + f" {output_shp} ../ogr/data/poly.shp")
    gdaltest.runexternal(ogr2ogr_path + f" -append {output_shp} ../ogr/data/poly.shp")

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 20, "-append failed"
    ds = None

    gdaltest.runexternal(
        ogr2ogr_path + f" -overwrite {output_shp} ../ogr/data/poly.shp"
    )

    ds = ogr.Open(output_shp)
    assert (
        ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ), "-overwrite failed"
    ds = None


###############################################################################
# Test -explodecollections


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_33(ogr2ogr_path, tmp_path):

    src_csv = str(tmp_path / "test_ogr2ogr_33_src.csv")
    dst_shp = str(tmp_path / "test_ogr2ogr_33_dst.shp")

    f = open(src_csv, "wt")
    f.write("foo,WKT\n")
    f.write(
        'bar,"MULTIPOLYGON (((10 10,10 11,11 11,11 10,10 10)),((100 100,100 200,200 200,200 100,100 100),(125 125,175 125,175 175,125 175,125 125)))"\n'
    )
    f.write('baz,"POLYGON ((0 0,0 1,1 1,1 0,0 0))"\n')
    f.close()

    gdaltest.runexternal(
        ogr2ogr_path + f" -explodecollections {dst_shp} {src_csv} -select foo"
    )

    ds = ogr.Open(dst_shp)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 3, "-explodecollections failed"

    feat = lyr.GetFeature(0)
    assert feat.GetField("foo") == "bar"
    assert (
        feat.GetGeometryRef().ExportToWkt()
        == "POLYGON ((10 10,10 11,11 11,11 10,10 10))"
    )

    feat = lyr.GetFeature(1)
    assert feat.GetField("foo") == "bar"
    assert (
        feat.GetGeometryRef().ExportToWkt()
        == "POLYGON ((100 100,100 200,200 200,200 100,100 100),(125 125,175 125,175 175,125 175,125 125))"
    )

    feat = lyr.GetFeature(2)
    assert feat.GetField("foo") == "baz"
    assert feat.GetGeometryRef().ExportToWkt() == "POLYGON ((0 0,0 1,1 1,1 0,0 0))"

    ds = None


###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist src.shp -nln someDirThatDoesNotExist'
# This should result in creating a someDirThatDoesNotExist directory with
# someDirThatDoesNotExist.shp/dbf/shx inside this directory


def test_ogr2ogr_34(ogr2ogr_path, tmp_path):

    output_dir = str(tmp_path / "test_ogr2ogr_34_dir")

    gdaltest.runexternal(
        ogr2ogr_path + f" {output_dir} ../ogr/data/poly.shp -nln test_ogr2ogr_34_dir"
    )

    ds = ogr.Open(f"{output_dir}/test_ogr2ogr_34_dir.shp")
    assert (
        ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ), "initial shapefile creation failed"
    ds = None

    gdaltest.runexternal(
        ogr2ogr_path
        + f" -append {output_dir} ../ogr/data/poly.shp -nln test_ogr2ogr_34_dir"
    )

    ds = ogr.Open(f"{output_dir}/test_ogr2ogr_34_dir.shp")
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 20, "-append failed"
    ds = None

    gdaltest.runexternal(
        ogr2ogr_path
        + f" -overwrite {output_dir} ../ogr/data/poly.shp -nln test_ogr2ogr_34_dir"
    )

    ds = ogr.Open(f"{output_dir}/test_ogr2ogr_34_dir.shp")
    assert (
        ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ), "-overwrite failed"
    ds = None


###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist src.shp'


def test_ogr2ogr_35(ogr2ogr_path, tmp_path):

    output_dir = str(tmp_path / "test_ogr2ogr_35_dir")

    gdaltest.runexternal(ogr2ogr_path + f" {output_dir} ../ogr/data/poly.shp ")

    ds = ogr.Open(f"{output_dir}/poly.shp")
    assert (
        ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ), "initial shapefile creation failed"
    ds = None

    gdaltest.runexternal(ogr2ogr_path + f" -append {output_dir} ../ogr/data/poly.shp")

    ds = ogr.Open(f"{output_dir}/poly.shp")
    assert ds is not None and ds.GetLayer(0).GetFeatureCount() == 20, "-append failed"
    ds = None

    gdaltest.runexternal(
        ogr2ogr_path + f" -overwrite {output_dir} ../ogr/data/poly.shp"
    )

    ds = ogr.Open(f"{output_dir}/poly.shp")
    assert (
        ds is not None and ds.GetLayer(0).GetFeatureCount() == 10
    ), "-overwrite failed"
    ds = None


###############################################################################
# Test ogr2ogr -zfield


def test_ogr2ogr_36(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "test_ogr2ogr_36.shp")

    gdaltest.runexternal(
        ogr2ogr_path + f" {output_shp} ../ogr/data/poly.shp -zfield EAS_ID"
    )

    ds = ogr.Open(output_shp)
    feat = ds.GetLayer(0).GetNextFeature()
    wkt = feat.GetGeometryRef().ExportToWkt()
    ds = None

    assert wkt.find(" 168,") != -1


###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist.shp dataSourceWithMultipleLayer'


def test_ogr2ogr_37(ogr2ogr_path, tmp_path):

    src_dir = tmp_path / "test_ogr2ogr_37_src"
    output_shp = str(tmp_path / "test_ogr2ogr_37_dir.shp")

    src_dir.mkdir()
    shutil.copy("../ogr/data/poly.shp", src_dir)
    shutil.copy("../ogr/data/poly.shx", src_dir)
    shutil.copy("../ogr/data/poly.dbf", src_dir)
    shutil.copy("../ogr/data/shp/testpoly.shp", src_dir)
    shutil.copy("../ogr/data/shp/testpoly.shx", src_dir)
    shutil.copy("../ogr/data/shp/testpoly.dbf", src_dir)

    gdaltest.runexternal(f"{ogr2ogr_path} {output_shp} {src_dir}")

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayerCount() == 2
    ds = None


###############################################################################
# Test that we take into account the fields by the where clause when combining
# -select and -where (#4015)


def test_ogr2ogr_38(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "test_ogr2ogr_38.shp")

    gdaltest.runexternal(
        ogr2ogr_path
        + f' {output_shp} ../ogr/data/poly.shp -select AREA -where "EAS_ID = 170"'
    )

    ds = ogr.Open(output_shp)
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat is not None
    ds = None


###############################################################################
# Test 'ogr2ogr someDirThatDoesNotExist.shp dataSourceWithMultipleLayer -sql "select * from alayer"' (#4268)


def test_ogr2ogr_39(ogr2ogr_path, tmp_path):

    src_dir = tmp_path / "test_ogr2ogr_39_src"
    output_shp = str(tmp_path / "test_ogr2ogr_39_dir.shp")

    src_dir.mkdir()
    shutil.copy("../ogr/data/poly.shp", src_dir)
    shutil.copy("../ogr/data/poly.shx", src_dir)
    shutil.copy("../ogr/data/poly.dbf", src_dir)
    shutil.copy("../ogr/data/shp/testpoly.shp", src_dir)
    shutil.copy("../ogr/data/shp/testpoly.shx", src_dir)
    shutil.copy("../ogr/data/shp/testpoly.dbf", src_dir)

    gdaltest.runexternal(
        f'{ogr2ogr_path} {output_shp} {src_dir} -sql "select * from poly"'
    )

    ds = ogr.Open(output_shp)
    assert ds is not None and ds.GetLayerCount() == 1
    ds = None


###############################################################################
# Test 'ogr2ogr -update asqlite.db asqlite.db layersrc -nln layerdst' (#4270)


@pytest.mark.require_driver("SQLite")
def test_ogr2ogr_40(ogr2ogr_path, tmp_path):

    output_db = str(tmp_path / "test_ogr2ogr_40.db")

    gdaltest.runexternal(f"{ogr2ogr_path} -f SQlite {output_db} ../ogr/data/poly.shp")
    gdaltest.runexternal(
        f"{ogr2ogr_path} -update {output_db} {output_db} poly -nln poly2"
    )

    ds = ogr.Open(output_db)
    lyr = ds.GetLayerByName("poly2")
    assert lyr.GetFeatureCount() == 10
    ds = None


###############################################################################
# Test 'ogr2ogr -update PG:xxxx PG:xxxx layersrc -nln layerdst' (#4270)


def test_ogr2ogr_41(ogr2ogr_path, pg_ds):

    lyr = pg_ds.CreateLayer("test_ogr2ogr_41_src")
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))
    lyr.StartTransaction()
    for i in range(501):
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat["foo"] = "%d" % i
        lyr.CreateFeature(feat)
        feat = None
    lyr.CommitTransaction()
    lyr = None

    ret = gdaltest.runexternal(
        f"{ogr2ogr_path} -update '{pg_ds.GetDescription()}' '{pg_ds.GetDescription()}' test_ogr2ogr_41_src -nln test_ogr2ogr_41_target"
    )
    assert "ERROR" not in ret

    lyr = pg_ds.GetLayerByName("test_ogr2ogr_41_target")
    assert lyr.GetFeatureCount() == 501


###############################################################################
# Test combination of -select and -where FID=xx (#4500)


def test_ogr2ogr_42(ogr2ogr_path, tmp_path):

    output_shp = str(tmp_path / "test_ogr2ogr_42.shp")

    gdaltest.runexternal(
        f'{ogr2ogr_path} {output_shp} ../ogr/data/poly.shp -select AREA -where "FID = 0"'
    )

    ds = ogr.Open(output_shp)
    lyr = ds.GetLayerByIndex(0)
    assert lyr.GetFeatureCount() == 1
    ds = None


###############################################################################
# Test -dim 3 and -dim 2


@pytest.mark.parametrize("dim", (2, 3))
def test_ogr2ogr_43(ogr2ogr_path, tmp_path, dim):

    output_shp = str(tmp_path / "test_ogr2ogr_43_{dim}d.shp")

    gdaltest.runexternal(f"{ogr2ogr_path} {output_shp} ../ogr/data/poly.shp -dim {dim}")

    ds = ogr.Open(output_shp)
    lyr = ds.GetLayerByIndex(0)
    if dim == 3:
        assert lyr.GetGeomType() == ogr.wkbPolygon25D
    elif dim == 2:
        assert lyr.GetGeomType() == ogr.wkbPolygon


###############################################################################
# Test -nlt PROMOTE_TO_MULTI for polygon/multipolygon


@pytest.mark.require_driver("GML")
def test_ogr2ogr_44(ogr2ogr_path, tmp_path):

    src_shp = str(tmp_path / "test_ogr2ogr_44_src.shp")
    dst_gml = str(tmp_path / "test_ogr2ogr_44.xml")

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(src_shp)
    lyr = ds.CreateLayer("test_ogr2ogr_44_src", geom_type=ogr.wkbPolygon)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,0 0))"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "MULTIPOLYGON(((0 0,0 1,1 1,0 0)),((10 0,10 1,11 1,10 0)))"
        )
    )
    lyr.CreateFeature(feat)
    ds = None

    gdaltest.runexternal(
        f"{ogr2ogr_path} -f GML {dst_gml} {src_shp} -nlt PROMOTE_TO_MULTI"
    )

    f = open(dst_gml[:-4] + ".xsd")
    data = f.read()
    f.close()

    assert 'type="gml:MultiSurfacePropertyType"' in data

    f = open(dst_gml)
    data = f.read()
    f.close()

    assert (
        '<gml:MultiSurface gml:id="test_ogr2ogr_44_src.geom.0"><gml:surfaceMember><gml:Polygon gml:id="test_ogr2ogr_44_src.geom.0.0"><gml:exterior><gml:LinearRing><gml:posList>0 0 0 1 1 1 0 0</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></gml:surfaceMember></gml:MultiSurface>'
        in data
    )


###############################################################################
# Test -nlt PROMOTE_TO_MULTI for linestring/multilinestring


@pytest.mark.require_driver("GML")
def test_ogr2ogr_45(ogr2ogr_path, tmp_path):

    src_shp = str(tmp_path / "test_ogr2ogr_45_src.shp")
    dst_gml = str(tmp_path / "test_ogr2ogr_45.gml")

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(src_shp)
    lyr = ds.CreateLayer("test_ogr2ogr_45_src", geom_type=ogr.wkbLineString)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("LINESTRING(0 0,0 1,1 1,0 0)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(
        ogr.CreateGeometryFromWkt(
            "MULTILINESTRING((0 0,0 1,1 1,0 0),(10 0,10 1,11 1,10 0))"
        )
    )
    lyr.CreateFeature(feat)
    ds = None

    gdaltest.runexternal(
        f"{ogr2ogr_path} -f GML {dst_gml} {src_shp} -nlt PROMOTE_TO_MULTI"
    )

    f = open(dst_gml[:-4] + ".xsd")
    data = f.read()
    f.close()

    assert 'type="gml:MultiCurvePropertyType"' in data

    f = open(dst_gml)
    data = f.read()
    f.close()

    assert (
        '<gml:MultiCurve gml:id="test_ogr2ogr_45_src.geom.0"><gml:curveMember><gml:LineString gml:id="test_ogr2ogr_45_src.geom.0.0"><gml:posList>0 0 0 1 1 1 0 0</gml:posList></gml:LineString></gml:curveMember></gml:MultiCurve>'
        in data
    )


###############################################################################
# Test -gcp (#4604)


@pytest.mark.require_driver("GML")
@pytest.mark.parametrize(
    "option",
    [
        "",
        "-tps",
        "-order 1",
        "-a_srs EPSG:4326",
        "-s_srs EPSG:4326 -t_srs EPSG:3857",
    ],
)
def test_ogr2ogr_46(ogr2ogr_path, tmp_path, option):

    src_shp = str(tmp_path / "test_ogr2ogr_46_src.shp")
    dst_gml = str(tmp_path / "test_ogr2ogr_46.gml")

    ds = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource(src_shp)
    lyr = ds.CreateLayer("test_ogr2ogr_46_src", geom_type=ogr.wkbPoint)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT(1 1)"))
    lyr.CreateFeature(feat)
    ds = None

    gdaltest.runexternal(
        f"{ogr2ogr_path} -f GML -dsco FORMAT=GML2 {dst_gml} {src_shp} -gcp 0 0 2 49 -gcp 0 1 2 50 -gcp 1 0 3 49 {option}"
    )

    f = open(dst_gml)
    data = f.read()
    f.close()

    assert not (
        "2,49" not in data and "2.0,49.0" not in data and "222638." not in data
    ), option

    assert not (
        "3,50" not in data and "3.0,50.0" not in data and "333958." not in data
    ), option


###############################################################################
# Test reprojection with features with different SRS


@pytest.mark.require_driver("GML")
def test_ogr2ogr_47(ogr2ogr_path, tmp_path):

    src_gml = str(tmp_path / "test_ogr2ogr_47_src.gml")
    dst_gml = str(tmp_path / "test_ogr2ogr_47_dst.gml")

    f = open(src_gml, "wt")
    f.write("""<foo xmlns:gml="http://www.opengis.net/gml">
   <gml:featureMember>
      <features>
         <geometry>
            <gml:Point srsName="http://www.opengis.net/gml/srs/epsg.xml#32630">
               <gml:coordinates>500000,4500000</gml:coordinates>
            </gml:Point>
         </geometry>
      </features>
   </gml:featureMember>
   <gml:featureMember>
      <features >
         <geometry>
            <gml:Point srsName="http://www.opengis.net/gml/srs/epsg.xml#32631">
               <gml:coordinates>500000,4500000</gml:coordinates>
            </gml:Point>
         </geometry>
      </features>
   </gml:featureMember>
</foo>""")
    f.close()

    gdaltest.runexternal(
        f"{ogr2ogr_path} -f GML -dsco FORMAT=GML2 -t_srs EPSG:4326 {dst_gml} {src_gml}"
    )

    f = open(dst_gml)
    data = f.read()
    f.close()

    assert (
        (">-3.0,40.65" in data and ">3.0,40.65" in data)
        or (">-3,40.65" in data and ">3.0,40.65" in data)
        or (">-2.99999999999999,40.65" in data and ">2.99999999999999,40.65" in data)
    ), data


###############################################################################
# Test fieldmap option


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_48(ogr2ogr_path, tmp_path):

    gdaltest.runexternal(f"{ogr2ogr_path} {tmp_path} data/Fields.csv")
    gdaltest.runexternal(
        f"{ogr2ogr_path} -append -fieldmap identity {tmp_path} data/Fields.csv"
    )
    gdaltest.runexternal(
        f"{ogr2ogr_path} -append -fieldmap 14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 {tmp_path} data/Fields.csv"
    )

    ds = ogr.Open(f"{tmp_path}/Fields.dbf")

    assert ds is not None
    layer_defn = ds.GetLayer(0).GetLayerDefn()
    assert layer_defn.GetFieldCount() == 15, "Unexpected field count"

    lyr = ds.GetLayer(0)
    lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    for i in range(layer_defn.GetFieldCount()):
        assert feat.GetFieldAsString(i) == str(i + 1)

    feat = lyr.GetNextFeature()
    for i in range(layer_defn.GetFieldCount()):
        assert feat.GetFieldAsString(i) == str(layer_defn.GetFieldCount() - i)


###############################################################################
# Test detection of duplicated field names in source layer and renaming
# in target layer


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_49(ogr2ogr_path, tmp_path):

    output_csv = str(tmp_path / "tset_ogr2ogr_49.csv")

    gdaltest.runexternal(
        f"{ogr2ogr_path} -f CSV {output_csv} data/duplicatedfields.csv"
    )
    f = open(output_csv)
    lines = f.readlines()
    f.close()

    assert (
        lines[0].find("foo,bar,foo3,foo2,baz,foo4") == 0
        and lines[1].find("val_foo,val_bar,val_foo3,val_foo2,val_baz,val_foo4") == 0
    )


###############################################################################
# Test detection of duplicated field names is case insensitive (#5208)


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("KML")
def test_ogr2ogr_49_bis(ogr2ogr_path, tmp_path):

    output_kml = str(tmp_path / "test_ogr2ogr_49_bis.kml")

    gdaltest.runexternal(
        f'{ogr2ogr_path} -f KML {output_kml} data/grid.csv -sql "SELECT field_1 AS name FROM grid WHERE fid = 1"'
    )
    f = open(output_kml)
    lines = f.readlines()
    f.close()

    expected_lines = [
        """<?xml version="1.0" encoding="utf-8" ?>""",
        """<kml xmlns="http://www.opengis.net/kml/2.2">""",
        """<Document id="root_doc">""",
        """<Folder><name>grid</name>""",
        """  <Placemark id="grid.1">""",
        """        <name>440750.000</name>""",
        """  </Placemark>""",
        """</Folder>""",
        """</Document></kml>""",
    ]

    assert len(lines) == len(expected_lines)
    for i, line in enumerate(lines):
        assert line.strip() == expected_lines[i].strip(), lines


###############################################################################
# Test -addfields


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_50(ogr2ogr_path, tmp_path):

    src1_csv = str(tmp_path / "test_ogr2ogr_50_1.csv")
    src2_csv = str(tmp_path / "test_ogr2ogr_50_2.csv")
    dst_dbf = str(tmp_path / "test_ogr2ogr_50.dbf")

    f = open(src1_csv, "wt")
    f.write("id,field1\n")
    f.write("1,foo\n")
    f.close()

    f = open(src2_csv, "wt")
    f.write("id,field1,field2\n")
    f.write("2,bar,baz\n")
    f.close()

    gdaltest.runexternal(f"{ogr2ogr_path} {dst_dbf} {src1_csv} -nln test_ogr2ogr_50")
    gdaltest.runexternal(
        f"{ogr2ogr_path} -addfields {dst_dbf} {src2_csv} -nln test_ogr2ogr_50"
    )

    ds = ogr.Open(dst_dbf)
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetField("field1") != "foo" or not feat.IsFieldNull("field2"):
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if feat.GetField("field1") != "bar" or feat.GetField("field2") != "baz":
        feat.DumpReadable()
        pytest.fail()
    ds = None


###############################################################################
# Test RFC 41 support


@pytest.fixture()
def ogr2ogr_51_multigeom_csv(tmp_path):

    src_csv = str(tmp_path / "test_ogr2ogr_51_src.csv")

    with open(src_csv, "wt") as f:
        f.write("id,_WKTgeom1_EPSG_4326,foo,_WKTgeom2_EPSG_32631\n")
        f.write('1,"POINT(1 2)","bar","POINT(3 4)"\n')

    return src_csv


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_51(ogr2ogr_path, ogr2ogr_51_multigeom_csv, tmp_path):

    src_csv = ogr2ogr_51_multigeom_csv
    dst_csv = str(tmp_path / "test_ogr2ogr_51_dst.csv")

    # Test conversion from a multi-geometry format into a multi-geometry format
    gdaltest.runexternal(
        f"{ogr2ogr_path} -f CSV {dst_csv} {src_csv} -nln test_ogr2ogr_51_dst -dsco GEOMETRY=AS_WKT -lco STRING_QUOTING=ALWAYS"
    )

    f = open(dst_csv, "rt")
    lines = f.readlines()
    f.close()

    expected_lines = [
        '"_WKTgeom1_EPSG_4326","_WKTgeom2_EPSG_32631","id","foo"',
        '"POINT (1 2)","POINT (3 4)","1","bar"',
    ]
    for i in range(2):
        assert lines[i].strip() == expected_lines[i]

    # Test -append into a multi-geometry format
    gdaltest.runexternal(
        f"{ogr2ogr_path} -append {dst_csv} {src_csv} -nln test_ogr2ogr_51_dst"
    )

    f = open(dst_csv, "rt")
    lines = f.readlines()
    f.close()

    expected_lines = [
        '"_WKTgeom1_EPSG_4326","_WKTgeom2_EPSG_32631","id","foo"',
        '"POINT (1 2)","POINT (3 4)","1","bar"',
        '"POINT (1 2)","POINT (3 4)","1","bar"',
    ]
    for i in range(3):
        assert lines[i].strip() == expected_lines[i]


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_51bis(ogr2ogr_path, ogr2ogr_51_multigeom_csv, tmp_path):

    src_csv = ogr2ogr_51_multigeom_csv
    dst_shp = str(tmp_path / "test_ogr2ogr_51_dst.shp")

    # Test conversion from a multi-geometry format into a single-geometry format
    gdaltest.runexternal(f"{ogr2ogr_path} {dst_shp} {src_csv} -nln test_ogr2ogr_51_dst")

    ds = ogr.Open(dst_shp)
    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    assert sr is not None and sr.ExportToWkt().find('GEOGCS["WGS 84"') == 0
    feat = lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == "POINT (1 2)"
    ds = None


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_51ter(ogr2ogr_path, ogr2ogr_51_multigeom_csv, tmp_path):

    src_csv = ogr2ogr_51_multigeom_csv
    dst_csv = str(tmp_path / "test_ogr2ogr_51_dst.csv")

    # Test -select with geometry field names
    gdaltest.runexternal(
        f"{ogr2ogr_path} -select foo,geom__WKTgeom2_EPSG_32631,id,geom__WKTgeom1_EPSG_4326 -f CSV {dst_csv} {src_csv} -nln test_ogr2ogr_51_dst -dsco GEOMETRY=AS_WKT -lco STRING_QUOTING=ALWAYS"
    )

    f = open(dst_csv, "rt")
    lines = f.readlines()
    f.close()

    expected_lines = [
        '"_WKTgeom2_EPSG_32631","_WKTgeom1_EPSG_4326","foo","id"',
        '"POINT (3 4)","POINT (1 2)","bar","1"',
    ]
    for i in range(2):
        assert lines[i].strip() == expected_lines[i]

    # Test -geomfield option
    gdaltest.runexternal(
        f"{ogr2ogr_path} -append {dst_csv} {src_csv} -nln test_ogr2ogr_51_dst -spat 1 2 1 2 -geomfield geom__WKTgeom1_EPSG_4326"
    )

    f = open(dst_csv, "rt")
    lines = f.readlines()
    f.close()

    expected_lines = [
        '"_WKTgeom2_EPSG_32631","_WKTgeom1_EPSG_4326","foo","id"',
        '"POINT (3 4)","POINT (1 2)","bar","1"',
        '"POINT (3 4)","POINT (1 2)","bar","1"',
    ]
    for i in range(2):
        assert lines[i].strip() == expected_lines[i]


###############################################################################
# Test -nlt CONVERT_TO_LINEAR and -nlt CONVERT_TO_CURVE


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_52(ogr2ogr_path, tmp_path):

    src_csv = str(tmp_path / "test_ogr2ogr_52_src.csv")
    dst1_csv = str(tmp_path / "test_ogr2ogr_52_dst.csv")
    dst2_csv = str(tmp_path / "test_ogr2ogr_52_dst2.csv")

    f = open(src_csv, "wt")
    f.write("id,WKT\n")
    f.write('1,"CIRCULARSTRING(0 0,1 0,0 0)"\n')
    f.close()

    gdaltest.runexternal(
        f"{ogr2ogr_path}  -f CSV {dst1_csv} {src_csv} -select id -nln test_ogr2ogr_52_dst -dsco GEOMETRY=AS_WKT -nlt CONVERT_TO_LINEAR"
    )

    f = open(dst1_csv, "rt")
    content = f.read()
    f.close()

    assert "LINESTRING (0 0," in content

    gdaltest.runexternal(
        f"{ogr2ogr_path} -f CSV {dst2_csv} {dst1_csv} -select id -nln test_ogr2ogr_52_dst2 -dsco GEOMETRY=AS_WKT -nlt CONVERT_TO_CURVE"
    )

    f = open(dst2_csv, "rt")
    content = f.read()
    f.close()

    assert "COMPOUNDCURVE ((0 0," in content


###############################################################################
# Test -mapFieldType and 64 bit integers


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("KML")
def test_ogr2ogr_53(ogr2ogr_path, tmp_path):

    src_csv = str(tmp_path / "test_ogr2ogr_53.csv")
    src_csvt = str(tmp_path / "test_ogr2ogr_53.csvt")
    dst_kml = str(tmp_path / "test_ogr2ogr_53.kml")
    dst2_kml = str(tmp_path / "test_ogr2ogr_53b.kml")

    f = open(src_csv, "wt")
    f.write("id,i64,b,WKT\n")
    f.write('1,123456789012,true,"POINT(0 0)"\n')
    f.close()
    f = open(src_csvt, "wt")
    f.write("Integer,Integer64,Integer(Boolean),String\n")
    f.close()

    # Default behaviour with a driver that declares GDAL_DMD_CREATIONFIELDDATATYPES
    gdaltest.runexternal(
        f'{ogr2ogr_path} -f KML {dst_kml} {src_csv} -mapFieldType "Integer(Boolean)=String"'
    )

    f = open(dst_kml, "rt")
    content = f.read()
    f.close()

    assert (
        '<SimpleField name="id" type="int"></SimpleField>' in content
        and '<SimpleData name="id">1</SimpleData>' in content
        and '<SimpleField name="i64" type="float"></SimpleField>' in content
        and '<SimpleData name="i64">123456789012</SimpleData>' in content
        and '<SimpleField name="b" type="string"></SimpleField>' in content
        and '<SimpleData name="b">1</SimpleData>' in content
    )

    # Default behaviour with a driver that does not GDAL_DMD_CREATIONFIELDDATATYPES
    # gdaltest.runexternal(ogr2ogr_path + ' -f BNA tmp/test_ogr2ogr_53.bna tmp/test_ogr2ogr_53.csv -nlt POINT')

    # f = open('tmp/test_ogr2ogr_53.bna', 'rt')
    # content = f.read()
    # f.close()

    # assert '"123456789012.0"' in content

    # os.unlink('tmp/test_ogr2ogr_53.bna')

    # with -mapFieldType
    gdaltest.runexternal(
        f"{ogr2ogr_path} -f KML {dst2_kml} {src_csv} -mapFieldType Integer64=String"
    )

    f = open(dst2_kml, "rt")
    content = f.read()
    f.close()

    assert (
        '<SimpleField name="i64" type="string"></SimpleField>' in content
        and '<SimpleData name="i64">123456789012</SimpleData>' in content
    )


###############################################################################
# Test behaviour with nullable fields


@pytest.fixture()
def ogr2ogr_54_vrt(tmp_path):
    src_csv = str(tmp_path / "test_ogr2ogr_54.csv")
    src_vrt = str(tmp_path / "test_ogr2ogr_54.vrt")

    f = open(src_csv, "wt")
    f.write("fld1,fld2,WKT\n")
    f.write('1,2,"POINT(0 0)"\n')
    f.close()

    f = open(src_vrt, "wt")
    f.write("""<OGRVRTDataSource>
  <OGRVRTLayer name="test_ogr2ogr_54">
    <SrcDataSource relativeToVRT="1" shared="1">test_ogr2ogr_54.csv</SrcDataSource>
    <SrcLayer>test_ogr2ogr_54</SrcLayer>
    <GeometryType>wkbUnknown</GeometryType>
    <GeometryField name="WKT" nullable="false"/>
    <Field name="fld1" type="String" src="fld1" nullable="no"/>
    <Field name="fld2" type="String" src="fld2"/>
  </OGRVRTLayer>
</OGRVRTDataSource>
""")
    f.close()

    return src_vrt


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("GML")
def test_ogr2ogr_54(ogr2ogr_path, ogr2ogr_54_vrt, tmp_path):

    gdaltest.runexternal(
        f"{ogr2ogr_path} -f GML {tmp_path}/test_ogr2ogr_54.gml {ogr2ogr_54_vrt}"
    )

    f = open(f"{tmp_path}/test_ogr2ogr_54.xsd", "rt")
    content = f.read()
    f.close()

    assert (
        '<xs:element name="WKT" type="gml:GeometryPropertyType" nillable="true" minOccurs="1" maxOccurs="1"/>'
        in content
        and '<xs:element name="fld1" nillable="true" minOccurs="1" maxOccurs="1">'
        in content
        and '<xs:element name="fld2" nillable="true" minOccurs="0" maxOccurs="1">'
        in content
    )


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("GML")
def test_ogr2ogr_54bis(ogr2ogr_path, ogr2ogr_54_vrt, tmp_path):

    # Test -forceNullable
    gdaltest.runexternal(
        f"{ogr2ogr_path} -forceNullable -f GML {tmp_path}/test_ogr2ogr_54.gml {ogr2ogr_54_vrt}"
    )

    f = open(f"{tmp_path}/test_ogr2ogr_54.xsd", "rt")
    content = f.read()
    f.close()

    assert (
        '<xs:element name="WKT" type="gml:GeometryPropertyType" nillable="true" minOccurs="0" maxOccurs="1"/>'
        in content
        and '<xs:element name="fld1" nillable="true" minOccurs="0" maxOccurs="1">'
        in content
        and '<xs:element name="fld2" nillable="true" minOccurs="0" maxOccurs="1">'
        in content
    )


###############################################################################
# Test behaviour with default values


@pytest.fixture()
def ogr2ogr_55_vrt(tmp_path):

    with open(f"{tmp_path}/test_ogr2ogr_55.csv", "wt") as f:
        f.write("fld1,fld2,WKT\n")
        f.write('1,,"POINT(0 0)"\n')

    with open(f"{tmp_path}/test_ogr2ogr_55.csvt", "wt") as f:
        f.write("Integer,Integer,String\n")

    with open(f"{tmp_path}/test_ogr2ogr_55.vrt", "wt") as f:
        f.write("""<OGRVRTDataSource>
      <OGRVRTLayer name="test_ogr2ogr_55">
        <SrcDataSource relativeToVRT="1" shared="1">test_ogr2ogr_55.csv</SrcDataSource>
        <SrcLayer>test_ogr2ogr_55</SrcLayer>
        <GeometryType>wkbUnknown</GeometryType>
        <GeometryField name="WKT"/>
        <Field name="fld1" type="Integer" src="fld1"/>
        <Field name="fld2" type="Integer" src="fld2" nullable="false" default="2"/>
      </OGRVRTLayer>
    </OGRVRTDataSource>
    """)

    return f"{tmp_path}/test_ogr2ogr_55.vrt"


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("GML")
def test_ogr2ogr_55(ogr2ogr_path, ogr2ogr_55_vrt, tmp_path):

    gdaltest.runexternal(
        f"{ogr2ogr_path} -f GML {tmp_path}/test_ogr2ogr_55.gml {ogr2ogr_55_vrt}"
    )

    f = open(f"{tmp_path}/test_ogr2ogr_55.gml", "rt")
    content = f.read()
    f.close()

    assert "<ogr:fld2>2</ogr:fld2>" in content


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("GML")
def test_ogr2ogr_55bis(ogr2ogr_path, ogr2ogr_55_vrt, tmp_path):

    # Test -unsetDefault
    gdaltest.runexternal(
        f"{ogr2ogr_path} -forceNullable -unsetDefault -f GML {tmp_path}/test_ogr2ogr_55.gml {ogr2ogr_55_vrt}"
    )

    f = open(f"{tmp_path}/test_ogr2ogr_55.gml", "rt")
    content = f.read()
    f.close()

    assert "<ogr:fld2>" not in content


###############################################################################
# Test behaviour when creating a field with same name as FID column.


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("PGDump")
def test_ogr2ogr_56(ogr2ogr_path, tmp_path):

    src_csv = str(tmp_path / "test_ogr2ogr_56.csv")
    src_csvt = str(tmp_path / "test_ogr2ogr_56.csvt")
    dst_sql = str(tmp_path / "test_ogr2ogr_56.sql")

    f = open(src_csv, "wt")
    f.write("str,myid,WKT\n")
    f.write('aaa,10,"POINT(0 0)"\n')
    f.close()

    f = open(src_csvt, "wt")
    f.write("String,Integer,String\n")
    f.close()

    gdaltest.runexternal(
        f"{ogr2ogr_path} -f PGDump {dst_sql} {src_csv} -lco FID=myid --config PGDUMP_DEBUG_ALLOW_CREATION_FIELD_WITH_FID_NAME=NO"
    )

    f = open(dst_sql, "rt")
    content = f.read()
    f.close()

    assert (
        """ALTER TABLE "public"."test_ogr2ogr_56" ADD COLUMN "myid" INT"""
        not in content
    )
    assert (
        """INSERT INTO "public"."test_ogr2ogr_56" ("myid", "wkb_geometry", "str", "wkt") VALUES (10, '010100000000000000000000000000000000000000', 'aaa', 'POINT(0 0)');"""
        in content
    )


###############################################################################
# Test default propagation of FID column name and values, and -unsetFid


@pytest.fixture()
def ogr2ogr_57_vrt(tmp_path):
    with open(f"{tmp_path}/test_ogr2ogr_57.csv", "wt") as f:
        f.write("id,str,WKT\n")
        f.write('10,a,"POINT(0 0)"\n')

    with open(f"{tmp_path}/test_ogr2ogr_57.csvt", "wt") as f:
        f.write("Integer,String,String\n")

    with open(f"{tmp_path}/test_ogr2ogr_57.vrt", "wt") as f:
        f.write("""<OGRVRTDataSource>
      <OGRVRTLayer name="test_ogr2ogr_57">
        <SrcDataSource relativeToVRT="1" shared="1">test_ogr2ogr_57.csv</SrcDataSource>
        <SrcLayer>test_ogr2ogr_57</SrcLayer>
        <GeometryType>wkbUnknown</GeometryType>
        <GeometryField name="WKT"/>
        <FID name="id">id</FID>
        <Field name="str"/>
      </OGRVRTLayer>
    </OGRVRTDataSource>
    """)

    return f"{tmp_path}/test_ogr2ogr_57.vrt"


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("PGDump")
def test_ogr2ogr_57(ogr2ogr_path, ogr2ogr_57_vrt, tmp_path):

    dst_sql = str(tmp_path / "test_ogr2ogr_57.sql")

    gdaltest.runexternal(f"{ogr2ogr_path} -f PGDump {dst_sql} {ogr2ogr_57_vrt}")

    f = open(dst_sql, "rt")
    content = f.read()
    f.close()

    assert (
        """ALTER TABLE "public"."test_ogr2ogr_57" ADD COLUMN "id" SERIAL CONSTRAINT "test_ogr2ogr_57_pk" PRIMARY KEY;"""
        in content
    )
    assert (
        """INSERT INTO "public"."test_ogr2ogr_57" ("id", "wkt", "str") VALUES (10, '010100000000000000000000000000000000000000', 'a');"""
        in content
    )


@pytest.mark.require_driver("CSV")
@pytest.mark.require_driver("PGDump")
def test_ogr2ogr_57bis(ogr2ogr_path, ogr2ogr_57_vrt, tmp_path):

    dst_sql = str(tmp_path / "test_ogr2ogr_57bis.sql")

    # Test -unsetFid
    gdaltest.runexternal(
        f"{ogr2ogr_path} -f PGDump {dst_sql} {ogr2ogr_57_vrt} -unsetFid"
    )

    f = open(dst_sql, "rt")
    content = f.read()
    f.close()

    assert (
        """ALTER TABLE "public"."test_ogr2ogr_57" ADD COLUMN "ogc_fid" SERIAL CONSTRAINT "test_ogr2ogr_57_pk" PRIMARY KEY;"""
        in content
    )
    assert (
        """INSERT INTO "public"."test_ogr2ogr_57" ("wkt", "str") VALUES ('010100000000000000000000000000000000000000', 'a');"""
        in content
    )


###############################################################################
# Test datasource transactions


@pytest.mark.require_driver("SQLite")
def test_ogr2ogr_58(ogr2ogr_path, tmp_path):

    dst_sqlite = str(tmp_path / "test_ogr2ogr_58.sqlite")

    gdaltest.runexternal(
        f"{ogr2ogr_path} -gt 3 -f SQLite {dst_sqlite} ../ogr/data/poly.shp"
    )

    ds = ogr.Open(dst_sqlite)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    ds = None


###############################################################################
# Test metadata support


@pytest.fixture()
def ogr2ogr_59_gpkg(tmp_path):

    src_gpkg = str(tmp_path / "test_ogr2ogr_59_src.gpkg")

    ds = ogr.GetDriverByName("GPKG").CreateDataSource(src_gpkg)
    ds.SetMetadataItem("FOO", "BAR")
    ds.SetMetadataItem("BAR", "BAZ", "another_domain")
    lyr = ds.CreateLayer("mylayer")
    lyr.SetMetadataItem("lyr_FOO", "lyr_BAR")
    lyr.SetMetadataItem("lyr_BAR", "lyr_BAZ", "lyr_another_domain")
    ds = None

    return src_gpkg


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_59(ogr2ogr_path, ogr2ogr_59_gpkg, tmp_path):

    dst_gpkg = str(tmp_path / "test_ogr2ogr_59_dest.gpkg")

    gdaltest.runexternal(
        f"{ogr2ogr_path} -f GPKG {dst_gpkg} {ogr2ogr_59_gpkg} -mo BAZ=BAW"
    )

    ds = ogr.Open(dst_gpkg)
    assert ds.GetMetadata() == {"FOO": "BAR", "BAZ": "BAW"}
    assert ds.GetMetadata("another_domain") == {"BAR": "BAZ"}
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadata() == {"lyr_FOO": "lyr_BAR"}
    assert lyr.GetMetadata("lyr_another_domain") == {"lyr_BAR": "lyr_BAZ"}
    ds = None


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_59bis(ogr2ogr_path, ogr2ogr_59_gpkg, tmp_path):

    dst_gpkg = str(tmp_path / "test_ogr2ogr_59bis_dest.gpkg")

    gdaltest.runexternal(f"{ogr2ogr_path} -f GPKG {dst_gpkg} {ogr2ogr_59_gpkg} -nomd")
    ds = ogr.Open(dst_gpkg)
    assert ds.GetMetadata() == {}
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadata() == {}
    ds = None


###############################################################################
# Test forced datasource transactions


@pytest.mark.require_driver("OpenFileGDB")
def test_ogr2ogr_60(ogr2ogr_path, tmp_path):

    dst_gdb = str(tmp_path / "test_ogr2ogr_60.gdb")

    gdaltest.runexternal(
        f"{ogr2ogr_path} -ds_transaction -f OpenFileGDB {dst_gdb} ../ogr/data/poly.shp -mapFieldType Integer64=Integer"
    )

    ds = ogr.Open(dst_gdb)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 10
    ds = None


###############################################################################
# Test -spat_srs


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_61(ogr2ogr_path, tmp_path):

    src_csv = str(tmp_path / "test_ogr2ogr_61.csv")
    dst_shp = str(tmp_path / "test_ogr2ogr_61.shp")
    dst2_shp = str(tmp_path / "test_ogr2ogr_61b.shp")

    f = open(src_csv, "wt")
    f.write("foo,WKT\n")
    f.write('1,"POINT(2 49)"\n')
    f.close()

    gdaltest.runexternal(
        f"{ogr2ogr_path} {dst_shp} {src_csv} -spat 426857 5427937 426858 5427938 -spat_srs EPSG:32631 -s_srs EPSG:4326 -a_srs EPSG:4326"
    )

    with ogr.Open(dst_shp) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1

    gdaltest.runexternal(
        f"{ogr2ogr_path} {dst2_shp} {dst_shp} -spat 426857 5427937 426858 5427938 -spat_srs EPSG:32631"
    )

    with ogr.Open(dst2_shp) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 1


###############################################################################
# Test -noNativeData


@pytest.fixture()
def ogr2ogr_62_json(tmp_path):

    fname = str(tmp_path / "test_ogr2ogr_62_in.json")

    with open(fname, "wt") as fp:
        fp.write(
            '{"type": "FeatureCollection", "foo": "bar", "features":[ { "type": "Feature", "bar": "baz", "properties": { "myprop": "myvalue" }, "geometry": null } ]}'
        )

    return fname


@pytest.mark.require_driver("GeoJSON")
def test_ogr2ogr_62(ogr2ogr_path, ogr2ogr_62_json, tmp_path):

    dst_json = str(tmp_path / "test_ogr2ogr_62.json")

    # Default behaviour

    gdaltest.runexternal(f"{ogr2ogr_path} -f GeoJSON {dst_json} {ogr2ogr_62_json}")
    fp = gdal.VSIFOpenL(dst_json, "rb")
    assert fp is not None
    data = gdal.VSIFReadL(1, 10000, fp).decode("ascii")
    gdal.VSIFCloseL(fp)

    assert "bar" in data and "baz" in data


@pytest.mark.require_driver("GeoJSON")
def test_ogr2ogr_62bis(ogr2ogr_path, ogr2ogr_62_json, tmp_path):

    dst_json = str(tmp_path / "test_ogr2ogr_62bis.json")

    # Test -noNativeData
    gdaltest.runexternal(
        f"{ogr2ogr_path} -f GeoJSON {dst_json} {ogr2ogr_62_json} -noNativeData"
    )
    fp = gdal.VSIFOpenL(dst_json, "rb")
    assert fp is not None
    data = gdal.VSIFReadL(1, 10000, fp).decode("ascii")
    gdal.VSIFCloseL(fp)

    assert "bar" not in data and "baz" not in data


###############################################################################
# Test --formats


def test_ogr2ogr_63(ogr2ogr_path):

    ret, err = gdaltest.runexternal_out_and_err(ogr2ogr_path + " --formats")
    assert "Supported Formats" in ret, err
    assert "ERROR" not in err, ret


###############################################################################
# Test appending multiple layers, whose one already exists (#6345)


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_64(ogr2ogr_path, tmp_path):

    in_csv = str(tmp_path / "in_csv")
    out_csv = str(tmp_path / "out_csv")

    os.mkdir(in_csv)
    open(f"{in_csv}/lyr1.csv", "wt").write("id,col\n1,1\n")
    open(f"{in_csv}/lyr2.csv", "wt").write("id,col\n1,1\n")

    ds = ogr.Open(in_csv)
    first_layer = ds.GetLayer(0).GetName()
    second_layer = ds.GetLayer(1).GetName()
    ds = None

    gdaltest.runexternal(f"{ogr2ogr_path} -f CSV {out_csv} {in_csv} {second_layer}")
    gdaltest.runexternal(f"{ogr2ogr_path} -append {out_csv} {in_csv}")

    ds = ogr.Open(out_csv)
    assert ds.GetLayerByName(first_layer).GetFeatureCount() == 1
    assert ds.GetLayerByName(second_layer).GetFeatureCount() == 2
    ds = None


###############################################################################
# Test detection of extension


@pytest.mark.require_driver("CSV")
def test_ogr2ogr_65(ogr2ogr_path, tmp_path):

    dst_csv = str(tmp_path / "out.csv")

    gdaltest.runexternal(f"{ogr2ogr_path} {dst_csv} ../ogr/data/poly.shp")
    ds = gdal.OpenEx(dst_csv)
    assert ds.GetDriver().ShortName == "CSV"
    ds = None

    ret, err = gdaltest.runexternal_out_and_err(
        ogr2ogr_path + " /vsimem/out.xxx ../ogr/data/poly.shp"
    )
    if "Cannot guess" not in err:
        print(ret)
        pytest.fail("expected a warning about probably wrong extension")


###############################################################################
# Test accidental overriding of dataset when dst and src filenames are the same (#1465)


def test_ogr2ogr_66(ogr2ogr_path):

    ret, err = gdaltest.runexternal_out_and_err(
        ogr2ogr_path + " ../ogr/data/poly.shp ../ogr/data/poly.shp"
    )
    assert (
        "Source and destination datasets must be different in non-update mode" in err
    ), ret


def hexify_double(val):
    val = hex(val)
    # On 32bit Linux, we might get a trailing L
    return val.rstrip("L").lstrip("0x").zfill(16).upper()


###############################################################################
# Test coordinates values are preserved for identity transformations


# The x value is such that x * k * (1/k) != x with k the common factor used in degrees unit definition
# If the coordinates are converted to radians and back to degrees the value of x will be altered
@pytest.mark.parametrize("x,y,srid", [(float.fromhex("0x1.5EB3ED959A307p6"), 0, 4326)])
@pytest.mark.require_driver("CSV")
def test_ogr2ogr_check_identity_transformation(ogr2ogr_path, tmp_path, x, y, srid):
    import struct

    src_csv = str(tmp_path / "input_point.csv")
    dst1_shp = str(tmp_path / "output_point.shp")
    dst2_shp = str(tmp_path / "output_point2.shp")

    # Generate CSV file with test point
    xy_wkb = "0101000000" + "".join(
        hexify_double(q) for q in struct.unpack(">QQ", struct.pack("<dd", x, y))
    )
    f = open(src_csv, "wt")
    f.write("id,wkb_geom\n")
    f.write("1," + xy_wkb + "\n")
    f.close()

    # To check that the transformed values are identical to the original ones we need
    # to use a binary format with the same accuracy as the source (WKB).
    # CSV cannot be used for this purpose because WKB is not supported as a geometry output format.

    # Note that when transforming CSV to SHP the same internal definition of EPSG:srid is being used for source and target,
    # so that this transformation will have identically defined input and output units
    gdaltest.runexternal(
        f"{ogr2ogr_path} {dst1_shp} {src_csv} -oo GEOM_POSSIBLE_NAMES=wkb_geom -s_srs EPSG:{srid} -t_srs EPSG:{srid}"
    )

    with ogr.Open(dst1_shp) as ds:
        feat = ds.GetLayer(0).GetNextFeature()
        assert feat.GetGeometryRef().GetX() == x and feat.GetGeometryRef().GetY() == y

    # Now, transforming SHP to SHP will have a different definition of the SRS (EPSG:srid) which comes from the previously saved .prj file
    # For angular units in degrees the .prj is saved with greater precision than the internally used value.
    # We perform this additional transformation to exercise the case of units defined with different precision
    gdaltest.runexternal(f"{ogr2ogr_path} {dst2_shp} {dst1_shp} -t_srs EPSG:{srid}")
    ds = ogr.Open(dst2_shp)
    feat = ds.GetLayer(0).GetNextFeature()

    assert feat.GetGeometryRef().GetX() == x and feat.GetGeometryRef().GetY() == y


###############################################################################
# Test -if


@pytest.mark.require_driver("GPKG")
def test_ogr2ogr_if_ok(ogr2ogr_path):

    ret, err = gdaltest.runexternal_out_and_err(
        ogr2ogr_path + " -if GPKG /vsimem/out.gpkg ../ogr/data/gpkg/2d_envelope.gpkg"
    )
    assert ret == ""
    assert err == ""


###############################################################################
# Test -if


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_driver("GeoJSON")
def test_ogr2ogr_if_ko(ogr2ogr_path):

    _, err = gdaltest.runexternal_out_and_err(
        ogr2ogr_path + " -if GeoJSON /vsimem/out.gpkg ../ogr/data/gpkg/2d_envelope.gpkg"
    )
    assert "Unable to open datasource" in err


###############################################################################
# Test https://github.com/OSGeo/gdal/issues/9497


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_driver("Parquet")
def test_ogr2ogr_parquet_dataset_limit(ogr2ogr_path, tmp_path):

    if gdal.GetDriverByName("PARQUET").GetMetadataItem("ARROW_DATASET") is None:
        pytest.skip("GDAL built without ARROW_DATASET support")

    out_filename = str(tmp_path / "out.gpkg")
    gdaltest.runexternal(
        ogr2ogr_path + f" -limit 1 {out_filename} ../ogr/data/parquet/partitioned_hive"
    )

    ds = ogr.Open(out_filename)
    assert ds.GetLayer(0).GetFeatureCount() == 1
