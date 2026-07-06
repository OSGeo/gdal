#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGRSpatialReference::importFromISISPVL().
# Author:   Oleg Alexandrov
#
###############################################################################
# Copyright (c) 2026, Oleg Alexandrov
#
# SPDX-License-Identifier: MIT
###############################################################################


import pytest

from osgeo import osr

###############################################################################
# The named ISIS projections, each fed as a Mapping group with non-zero
# parameters. The full output PROJ string is checked. The radii are unequal, so
# the datum also varies by projection: a forced sphere, an ellipsoid, or (for
# Equirectangular) the local radius derived from the center latitude.


@pytest.mark.parametrize(
    "projection,extra,proj4",
    [
        (
            "Equirectangular",
            "CenterLatitude = 15.0\nCenterLongitude = 30.0",
            "+proj=eqc +lat_ts=15 +lat_0=0 +lon_0=30 +x_0=0 +y_0=0 "
            "+R=3394839.81331631 +units=m +no_defs",
        ),
        (
            "SimpleCylindrical",
            "CenterLatitude = 15.0\nCenterLongitude = 30.0",
            "+proj=eqc +lat_ts=15 +lat_0=0 +lon_0=30 +x_0=0 +y_0=0 "
            "+R=3396190 +units=m +no_defs",
        ),
        (
            "Orthographic",
            "CenterLatitude = -20.0\nCenterLongitude = 60.0",
            "+proj=ortho +lat_0=-20 +lon_0=60 +x_0=0 +y_0=0 +R=3396190 "
            "+units=m +no_defs",
        ),
        (
            "Sinusoidal",
            "CenterLongitude = 25.0",
            "+proj=sinu +lon_0=25 +x_0=0 +y_0=0 +R=3396190 +units=m +no_defs",
        ),
        (
            "Mercator",
            "CenterLatitude = 0.0\nCenterLongitude = 30.0\nscaleFactor = 0.9",
            "+proj=merc +lon_0=30 +k=0.9 +x_0=0 +y_0=0 +a=3396190 "
            "+rf=169.894447223612 +units=m +no_defs",
        ),
        (
            "PolarStereographic",
            "CenterLatitude = 90.0\nCenterLongitude = 45.0\nscaleFactor = 0.994",
            "+proj=stere +lat_0=90 +lon_0=45 +k=0.994 +x_0=0 +y_0=0 +a=3396190 "
            "+rf=169.894447223612 +units=m +no_defs",
        ),
        (
            "TransverseMercator",
            "CenterLatitude = 5.0\nCenterLongitude = 15.0\nscaleFactor = 0.9996",
            "+proj=tmerc +lat_0=5 +lon_0=15 +k=0.9996 +x_0=0 +y_0=0 +a=3396190 "
            "+rf=169.894447223612 +units=m +no_defs",
        ),
        (
            "LambertConformal",
            "CenterLatitude = 40.0\nCenterLongitude = 10.0\n"
            "FirstStandardParallel = 30.0\nSecondStandardParallel = 50.0",
            "+proj=lcc +lat_0=40 +lon_0=10 +lat_1=30 +lat_2=50 +x_0=0 +y_0=0 "
            "+a=3396190 +rf=169.894447223612 +units=m +no_defs",
        ),
        (
            "PointPerspective",
            "CenterLatitude = -10.0\nCenterLongitude = -90.0\nDistance = 35000.0 <km>",
            "+proj=nsper +lat_0=-10 +lon_0=-90 +h=31603810 +x_0=0 +y_0=0 "
            "+R=3396190 +units=m +no_defs",
        ),
        (
            "ObliqueCylindrical",
            "PoleLatitude = 30.0\nPoleLongitude = 20.0\nPoleRotation = 15.0",
            "+proj=ob_tran +o_proj=eqc +o_lon_p=-15 +o_lat_p=150 +lon_0=20 "
            "+a=3396190 +rf=169.894447223612 +units=m +no_defs",
        ),
    ],
)
def test_osr_isis_named(projection, extra, proj4):

    srs = osr.SpatialReference()
    mapping = f"""Group = Mapping
  ProjectionName   = {projection}
  TargetName       = Mars
  EquatorialRadius = 3396190.0 <meters>
  PolarRadius      = 3376200.0 <meters>
  {extra}
End_Group"""
    assert srs.ImportFromISISPVL(mapping) == 0
    assert srs.IsProjected()
    assert srs.ExportToProj4().strip() == proj4


###############################################################################
# The generic IProj form, whose definition is carried by a ProjStr that may
# span several lines.


def test_osr_isis_projstr():

    srs = osr.SpatialReference()
    assert srs.ImportFromISISPVL("""Group = Mapping
  ProjectionName = IProj
  TargetName     = Mars
  ProjStr        = "+proj=eqc +lat_ts=0 +lat_0=0 +lon_0=0 +x_0=0 +y_0=0
                    +a=3396190 +b=3396190 +units=m +no_defs +type=crs"
End_Group""") == 0
    assert srs.IsProjected()
    assert srs.GetSemiMajor() == 3396190
    assert "+proj=eqc" in srs.ExportToProj4()


###############################################################################
# An unparseable ProjStr fails rather than crashing.


def test_osr_isis_invalid():

    srs = osr.SpatialReference()
    with pytest.raises(RuntimeError, match="ProjStr"):
        srs.ImportFromISISPVL("""Group = Mapping
  ProjectionName = IProj
  ProjStr        = "+proj=no_such_projection +a=3396190"
End_Group""")
