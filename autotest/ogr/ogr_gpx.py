#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GPX driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import ogrtest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("GPX")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    # Check that the GPX driver has read support
    with gdal.quiet_errors():
        if ogr.Open("data/gpx/test.gpx") is None:
            assert "Expat" in gdal.GetLastErrorMsg()
            pytest.skip("GDAL build without Expat support")

    yield


###############################################################################
# Test waypoints gpx layer.


def test_ogr_gpx_1():
    gpx_ds = ogr.Open("data/gpx/test.gpx")

    assert gpx_ds.GetLayerCount() == 5, "wrong number of layers"

    lyr = gpx_ds.GetLayerByName("waypoints")
    assert lyr.GetDataset().GetDescription() == gpx_ds.GetDescription()

    expect = [2, None]

    with gdal.quiet_errors():
        ogrtest.check_features_against_list(lyr, "ele", expect)

    lyr.ResetReading()

    expect = ["waypoint name", None]

    ogrtest.check_features_against_list(lyr, "name", expect)

    lyr.ResetReading()

    expect = ["href", None]

    ogrtest.check_features_against_list(lyr, "link1_href", expect)

    lyr.ResetReading()

    expect = ["text", None]

    ogrtest.check_features_against_list(lyr, "link1_text", expect)

    lyr.ResetReading()

    expect = ["type", None]

    ogrtest.check_features_against_list(lyr, "link1_type", expect)

    lyr.ResetReading()

    expect = ["href2", None]

    ogrtest.check_features_against_list(lyr, "link2_href", expect)

    lyr.ResetReading()

    expect = ["text2", None]

    ogrtest.check_features_against_list(lyr, "link2_text", expect)

    lyr.ResetReading()

    expect = ["type2", None]

    ogrtest.check_features_against_list(lyr, "link2_type", expect)

    lyr.ResetReading()

    expect = ["2007/11/25 17:58:00+01", None]

    ogrtest.check_features_against_list(lyr, "time", expect)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT (1 0)", max_error=0.0001)

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT (4 3)", max_error=0.0001)


###############################################################################
# Test routes gpx layer.


def test_ogr_gpx_2():
    gpx_ds = ogr.Open("data/gpx/test.gpx")

    lyr = gpx_ds.GetLayerByName("routes")

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING (6 5,9 8,12 11)", max_error=0.0001)

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING EMPTY", max_error=0.0001)


###############################################################################
# Test route_points gpx layer.


def test_ogr_gpx_3():
    gpx_ds = ogr.Open("data/gpx/test.gpx")

    lyr = gpx_ds.GetLayerByName("route_points")

    expect = ["route point name", None, None]

    ogrtest.check_features_against_list(lyr, "name", expect)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT (6 5)", max_error=0.0001)


###############################################################################
# Test tracks gpx layer.


def test_ogr_gpx_4():
    gpx_ds = ogr.Open("data/gpx/test.gpx")

    lyr = gpx_ds.GetLayerByName("tracks")

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "MULTILINESTRING ((15 14,18 17),(21 20,24 23))", max_error=0.0001
    )

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "MULTILINESTRING EMPTY", max_error=0.0001)

    feat = lyr.GetNextFeature()
    f_geom = feat.GetGeometryRef()
    assert f_geom.ExportToWkt() == "MULTILINESTRING EMPTY"


###############################################################################
# Test route_points gpx layer.


def test_ogr_gpx_5():
    gpx_ds = ogr.Open("data/gpx/test.gpx")

    lyr = gpx_ds.GetLayerByName("track_points")

    expect = ["track point name", None, None, None]

    ogrtest.check_features_against_list(lyr, "name", expect)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT (15 14)", max_error=0.0001)


###############################################################################
# Copy our small gpx file to a new gpx file.


def test_ogr_gpx_6(tmp_path):
    gpx_ds = ogr.Open("data/gpx/test.gpx")

    co_opts = []

    # Duplicate waypoints
    gpx_lyr = gpx_ds.GetLayerByName("waypoints")

    gpx2_ds = ogr.GetDriverByName("GPX").CreateDataSource(
        tmp_path / "gpx.gpx", options=co_opts
    )

    gpx2_lyr = gpx2_ds.CreateLayer("waypoints", geom_type=ogr.wkbPoint)

    gpx_lyr.ResetReading()

    dst_feat = ogr.Feature(feature_def=gpx2_lyr.GetLayerDefn())

    feat = gpx_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        assert gpx2_lyr.CreateFeature(dst_feat) == 0, "CreateFeature failed."

        feat = gpx_lyr.GetNextFeature()

    # Duplicate routes
    gpx_lyr = gpx_ds.GetLayerByName("routes")

    gpx2_lyr = gpx2_ds.CreateLayer("routes", geom_type=ogr.wkbLineString)

    gpx_lyr.ResetReading()

    dst_feat = ogr.Feature(feature_def=gpx2_lyr.GetLayerDefn())

    feat = gpx_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        assert gpx2_lyr.CreateFeature(dst_feat) == 0, "CreateFeature failed."

        feat = gpx_lyr.GetNextFeature()

    # Duplicate tracks
    gpx_lyr = gpx_ds.GetLayerByName("tracks")

    gpx2_lyr = gpx2_ds.CreateLayer("tracks", geom_type=ogr.wkbMultiLineString)

    gpx_lyr.ResetReading()

    dst_feat = ogr.Feature(feature_def=gpx2_lyr.GetLayerDefn())

    feat = gpx_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        assert gpx2_lyr.CreateFeature(dst_feat) == 0, "CreateFeature failed."

        feat = gpx_lyr.GetNextFeature()


###############################################################################
# Output extra fields as <extensions>.


def test_ogr_gpx_7(tmp_path):

    bna_ds = ogr.Open("data/gpx/csv_for_gpx.csv")

    co_opts = ["GPX_USE_EXTENSIONS=yes"]

    # Duplicate waypoints
    bna_lyr = bna_ds.GetLayerByName("csv_for_gpx")

    gpx_ds = ogr.GetDriverByName("GPX").CreateDataSource(
        tmp_path / "gpx.gpx", options=co_opts
    )

    gpx_lyr = gpx_ds.CreateLayer("waypoints", geom_type=ogr.wkbPoint)

    bna_lyr.ResetReading()

    for i in range(bna_lyr.GetLayerDefn().GetFieldCount()):
        field_defn = bna_lyr.GetLayerDefn().GetFieldDefn(i)
        gpx_lyr.CreateField(field_defn)

    dst_feat = ogr.Feature(feature_def=gpx_lyr.GetLayerDefn())

    feat = bna_lyr.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        assert gpx_lyr.CreateFeature(dst_feat) == 0, "CreateFeature failed."

        feat = bna_lyr.GetNextFeature()

    gpx_ds = None

    # Now check that the extensions fields have been well written
    gpx_ds = ogr.Open(tmp_path / "gpx.gpx")
    gpx_lyr = gpx_ds.GetLayerByName("waypoints")

    expect = ["PID1", "PID2"]

    ogrtest.check_features_against_list(gpx_lyr, "ogr_Primary_ID", expect)

    gpx_lyr.ResetReading()

    expect = ["SID1", "SID2"]

    ogrtest.check_features_against_list(gpx_lyr, "ogr_Secondary_ID", expect)

    gpx_lyr.ResetReading()

    expect = ["TID1", None]

    ogrtest.check_features_against_list(gpx_lyr, "ogr_Third_ID", expect)


###############################################################################
# Output extra fields as <extensions>.


def test_ogr_gpx_8(tmp_path):

    gpx_ds = ogr.GetDriverByName("GPX").CreateDataSource(
        tmp_path / "gpx.gpx", options=["LINEFORMAT=LF"]
    )

    lyr = gpx_ds.CreateLayer("route_points", geom_type=ogr.wkbPoint)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt("POINT(2 49)")
    feat.SetField("route_name", "ROUTE_NAME")
    feat.SetField("route_fid", 0)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt("POINT(3 50)")
    feat.SetField("route_name", "--ignored--")
    feat.SetField("route_fid", 0)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt("POINT(3 51)")
    feat.SetField("route_name", "ROUTE_NAME2")
    feat.SetField("route_fid", 1)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt("POINT(3 49)")
    feat.SetField("route_fid", 1)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    lyr = gpx_ds.CreateLayer("track_points", geom_type=ogr.wkbPoint)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt("POINT(2 49)")
    feat.SetField("track_name", "TRACK_NAME")
    feat.SetField("track_fid", 0)
    feat.SetField("track_seg_id", 0)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt("POINT(3 50)")
    feat.SetField("track_name", "--ignored--")
    feat.SetField("track_fid", 0)
    feat.SetField("track_seg_id", 0)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt("POINT(3 51)")
    feat.SetField("track_fid", 0)
    feat.SetField("track_seg_id", 1)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt("POINT(3 49)")
    feat.SetField("track_name", "TRACK_NAME2")
    feat.SetField("track_fid", 1)
    feat.SetField("track_seg_id", 0)
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    gpx_ds = None

    f = open(tmp_path / "gpx.gpx", "rb")
    f_ref = open("data/gpx/ogr_gpx_8_ref.txt", "rb")
    f_content = f.read()
    f_ref_content = f_ref.read()
    f.close()
    f_ref.close()

    assert f_ref_content.decode("utf-8") in f_content.decode(
        "utf-8"
    ), "did not get expected result"


###############################################################################
# Parse file with a <time> extension at track level (#6237)


def test_ogr_gpx_9():

    ds = ogr.Open("data/gpx/track_with_time_extension.gpx")
    lyr = ds.GetLayerByName("tracks")
    f = lyr.GetNextFeature()
    if f["time"] != "2015-10-11T15:06:33Z":
        f.DumpReadable()
        pytest.fail("did not get expected result")


###############################################################################
# Test reading metadata


def test_ogr_gpx_metadata_read():

    ds = ogr.Open("data/gpx/test.gpx")
    assert ds.GetMetadata() == {
        "AUTHOR_EMAIL": "foo@example.com",
        "AUTHOR_LINK_HREF": "author_href",
        "AUTHOR_LINK_TEXT": "author_text",
        "AUTHOR_LINK_TYPE": "author_type",
        "AUTHOR_NAME": "metadata author name",
        "COPYRIGHT_AUTHOR": "copyright author",
        "COPYRIGHT_LICENSE": "my license",
        "COPYRIGHT_YEAR": "2023",
        "DESCRIPTION": "metadata desc",
        "KEYWORDS": "kw",
        "LINK_1_HREF": "href",
        "LINK_1_TEXT": "text",
        "LINK_1_TYPE": "type",
        "LINK_2_HREF": "href2",
        "LINK_2_TEXT": "text3",
        "LINK_2_TYPE": "type3",
        "NAME": "metadata name",
        "TIME": "2007-11-25T17:58:00+01:00",
    }


###############################################################################
# Test writing metadata


def test_ogr_gpx_metadata_write(tmp_vsimem):

    md = {
        "AUTHOR_EMAIL": "foo@example.com",
        "AUTHOR_LINK_HREF": "author_href",
        "AUTHOR_LINK_TEXT": "author_text",
        "AUTHOR_LINK_TYPE": "author_type",
        "AUTHOR_NAME": "metadata author name",
        "COPYRIGHT_AUTHOR": "copyright author",
        "COPYRIGHT_LICENSE": "my license",
        "COPYRIGHT_YEAR": "2023",
        "DESCRIPTION": "metadata desc",
        "KEYWORDS": "kw",
        "LINK_1_HREF": "href",
        "LINK_1_TEXT": "text",
        "LINK_1_TYPE": "type",
        "LINK_2_HREF": "href2",
        "LINK_2_TEXT": "text3",
        "LINK_2_TYPE": "type3",
        "NAME": "metadata name",
        "TIME": "2007-11-25T17:58:00+01:00",
    }

    options = []
    for key in md:
        options.append("METADATA_" + key + "=" + md[key])

    gpx_ds = ogr.GetDriverByName("GPX").CreateDataSource(
        tmp_vsimem / "gpx.gpx", options=options
    )
    assert gpx_ds is not None
    gpx_ds = None

    ds = ogr.Open(tmp_vsimem / "gpx.gpx")
    # print(ds.GetMetadata())
    assert ds.GetMetadata() == md
    ds = None


###############################################################################
# Test CREATOR option


@pytest.mark.parametrize(
    "options,expected",
    [([], b' creator="GDAL '), (["CREATOR=the_creator"], b' creator="the_creator" ')],
)
def test_ogr_gpx_creator(tmp_vsimem, options, expected):

    filename = str(tmp_vsimem / "test_ogr_gpx_cerator.gpx")
    ogr.GetDriverByName("GPX").CreateDataSource(filename, options=options)
    assert ogr.Open(filename)
    f = gdal.VSIFOpenL(filename, "rb")
    data = gdal.VSIFReadL(1, gdal.VSIStatL(filename).size, f)
    gdal.VSIFCloseL(f)
    assert expected in data


###############################################################################
# Test ELE_AS_25D open option


def test_ogr_gpx_ELE_AS_25D():

    ds = gdal.OpenEx(
        "data/gpx/test.gpx", gdal.OF_VECTOR, open_options=["ELE_AS_25D=YES"]
    )
    lyr = ds.GetLayerByName("waypoints")
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == "POINT Z (1 0 2)"

    lyr = ds.GetLayerByName("routes")
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToIsoWkt() == "LINESTRING Z (6 5 7,9 8 10,12 11 13)"


###############################################################################
# Test SHORT_NAMES open option


def test_ogr_gpx_SHORT_NAMES():

    ds = gdal.OpenEx(
        "data/gpx/test.gpx", gdal.OF_VECTOR, open_options=["SHORT_NAMES=YES"]
    )
    lyr = ds.GetLayerByName("track_points")
    f = lyr.GetNextFeature()
    assert f["trksegid"] == 0


###############################################################################
# Test N_MAX_LINKS open option


def test_ogr_gpx_N_MAX_LINKS():

    ds = gdal.OpenEx(
        "data/gpx/test.gpx", gdal.OF_VECTOR, open_options=["N_MAX_LINKS=3"]
    )
    lyr = ds.GetLayerByName("track_points")
    f = lyr.GetNextFeature()
    assert f["link3_href"] is None


###############################################################################
# Test preservation of FID when converting to GPKG
# (https://github.com/OSGeo/gdal/issues/9225)


@pytest.mark.require_driver("GPKG")
def test_ogr_gpx_convert_to_gpkg(tmp_vsimem):

    outfilename = str(tmp_vsimem / "out.gpkg")
    gdal.VectorTranslate(outfilename, "data/gpx/test.gpx")

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer("tracks")
    f = lyr.GetNextFeature()
    assert f.GetFID() == 0
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1
