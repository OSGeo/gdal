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
# The named ISIS projections, each fed as a Mapping group.


@pytest.mark.parametrize(
    "projection,extra,proj4",
    [
        ("Equirectangular", "CenterLatitude = 0.0\nCenterLongitude = 0.0", "+proj=eqc"),
        (
            "SimpleCylindrical",
            "CenterLatitude = 0.0\nCenterLongitude = 0.0",
            "+proj=eqc",
        ),
        ("Orthographic", "CenterLatitude = 0.0\nCenterLongitude = 0.0", "+proj=ortho"),
        ("Sinusoidal", "CenterLongitude = 0.0", "+proj=sinu"),
        ("Mercator", "CenterLatitude = 0.0\nCenterLongitude = 0.0", "+proj=merc"),
        (
            "PolarStereographic",
            "CenterLatitude = 90.0\nCenterLongitude = 0.0",
            "+proj=stere",
        ),
        (
            "TransverseMercator",
            "CenterLatitude = 0.0\nCenterLongitude = 0.0",
            "+proj=tmerc",
        ),
        (
            "LambertConformal",
            "CenterLatitude = 40.0\nCenterLongitude = 0.0\n"
            "FirstStandardParallel = 30.0\nSecondStandardParallel = 50.0",
            "+proj=lcc",
        ),
        (
            "PointPerspective",
            "CenterLatitude = 0.0\nCenterLongitude = 0.0\nDistance = 5000.0 <km>",
            "+proj=nsper",
        ),
        (
            "ObliqueCylindrical",
            "PoleLatitude = 30.0\nPoleLongitude = 0.0\nPoleRotation = 0.0",
            "+proj=ob_tran",
        ),
    ],
)
def test_osr_isis_named(projection, extra, proj4):

    srs = osr.SpatialReference()
    mapping = f"""Group = Mapping
  ProjectionName   = {projection}
  TargetName       = Mars
  EquatorialRadius = 3396190.0 <meters>
  PolarRadius      = 3396190.0 <meters>
  {extra}
End_Group"""
    assert srs.ImportFromISISPVL(mapping) == 0
    assert srs.IsProjected()
    assert srs.GetSemiMajor() == pytest.approx(3396190)
    assert proj4 in srs.ExportToProj4()


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
