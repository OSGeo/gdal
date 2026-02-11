#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR GPSBabel driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal, ogr


def gpsbabel_binary_found():
    try:
        ret = gdaltest.runexternal("gpsbabel -V")
        return "GPSBabel" in ret
    except OSError:
        return False


pytestmark = [
    pytest.mark.require_driver("GPSBabel"),
    pytest.mark.require_driver("GPX"),
    pytest.mark.skipif(
        not gpsbabel_binary_found(), reason="GPSBabel utility not found"
    ),
]


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    # Check that the GPX driver has read support
    with gdal.quiet_errors():
        if ogr.Open("data/gpx/test.gpx") is None:
            assert "Expat" in gdal.GetLastErrorMsg()
            pytest.skip("GDAL build without Expat support")


###############################################################################
# Test reading with explicit subdriver


def test_ogr_gpsbabel_1():

    ds = ogr.Open("GPSBabel:nmea:data/gpsbabel/nmea.txt")
    assert ds is not None

    assert ds.GetLayerCount() == 2


###############################################################################
# Test reading with implicit subdriver


def test_ogr_gpsbabel_2():

    ds = ogr.Open("data/gpsbabel/nmea.txt")
    assert ds is not None

    assert ds.GetLayerCount() == 2


###############################################################################
# Test writing


def test_ogr_gpsbabel_3():

    ds = ogr.GetDriverByName("GPSBabel").CreateDataSource("GPSBabel:nmea:tmp/nmea.txt")
    lyr = ds.CreateLayer("track_points", geom_type=ogr.wkbPoint)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("track_fid", 0)
    feat.SetField("track_seg_id", 0)
    feat.SetField("track_name", "TRACK_NAME")
    feat.SetField("name", "PT_NAME")
    feat.SetField("hdop", 123)
    feat.SetField("vdop", 456)
    feat.SetField("pdop", 789)
    feat.SetField("sat", 6)
    feat.SetField("time", "2010/06/03 12:34:56")
    feat.SetField("fix", "3d")
    geom = ogr.CreateGeometryFromWkt("POINT(2.50 49.25)")
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    feat = None
    lyr = None
    ds = None

    f = open("tmp/nmea.txt", "rt")
    res = f.read()
    f.close()

    gdal.Unlink("tmp/nmea.txt")

    assert not (
        res.find("$GPRMC") == -1 or res.find("$GPGGA") == -1 or res.find("$GPGSA") == -1
    ), "did not get expected result"


###############################################################################
# Test features=


@pytest.mark.parametrize("features", ["waypoints", "tracks", "routes"])
def test_ogr_gpsbabel_features_in_connection_string(features):

    ds = ogr.Open(f"GPSBABEL:gpx:features={features}:data/gpx/test.gpx")
    assert ds
    assert ds.GetLayerCount() == (1 if features == "waypoints" else 2)
    assert ds.GetLayer(0).GetName() == features


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "connection_string",
    [
        "GPSBABEL:",
        "GPSBABEL:wrong_driver",
        "GPSBABEL:gpx:wrong_file",
        "GPSBABEL:gpx:features=wrong_feature:data/gpx/test.gpx",
        "GPSBABEL:gpx:features=waypoints",
    ],
)
def test_ogr_gpsbabel_bad_connection_string(connection_string):

    with pytest.raises(Exception):
        ogr.Open(connection_string)
