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
# A named ISIS projection.


def test_osr_isis_named():

    srs = osr.SpatialReference()
    assert srs.ImportFromISISPVL("""Group = Mapping
  ProjectionName   = Equirectangular
  TargetName       = Mars
  EquatorialRadius = 3396190.0 <meters>
  PolarRadius      = 3396190.0 <meters>
  CenterLongitude  = 0.0
  CenterLatitude   = 0.0
End_Group""") == 0
    assert srs.IsProjected()
    assert srs.GetSemiMajor() == 3396190
    assert "+proj=eqc" in srs.ExportToProj4()


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
# The Mapping-group wrapper is optional.


def test_osr_isis_no_wrapper():

    srs = osr.SpatialReference()
    assert srs.ImportFromISISPVL("""ProjectionName   = Sinusoidal
TargetName       = Mars
EquatorialRadius = 3396190.0 <meters>
PolarRadius      = 3396190.0 <meters>
CenterLongitude  = 0.0""") == 0
    assert "+proj=sinu" in srs.ExportToProj4()


###############################################################################
# An unparseable ProjStr fails rather than crashing.


def test_osr_isis_invalid():

    srs = osr.SpatialReference()
    with pytest.raises(RuntimeError, match="ProjStr"):
        srs.ImportFromISISPVL("""Group = Mapping
  ProjectionName = IProj
  ProjStr        = "+proj=no_such_projection +a=3396190"
End_Group""")
