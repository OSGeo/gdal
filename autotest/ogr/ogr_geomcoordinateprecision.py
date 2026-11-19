#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ogr.GeomCoordinatePrecision
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import ogr, osr


def test_ogr_geomcoordinate_precision():

    prec = ogr.CreateGeomCoordinatePrecision()
    assert prec.GetXYResolution() == 0
    assert prec.GetZResolution() == 0
    assert prec.GetMResolution() == 0

    prec.Set(1e-9, 1e-3, 1e-2)
    assert prec.GetXYResolution() == 1e-9
    assert prec.GetZResolution() == 1e-3
    assert prec.GetMResolution() == 1e-2

    with pytest.raises(Exception, match="Received a NULL pointer"):
        prec.SetFromMeter(None, 0, 0, 0)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    prec.SetFromMeter(srs, 1e-3, 1e-3, 1e-1)
    assert prec.GetXYResolution() == pytest.approx(8.983152841195213e-09)
    assert prec.GetZResolution() == 1e-3
    assert prec.GetMResolution() == 1e-1

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4979)
    prec.SetFromMeter(srs, 1e-3, 1e-3, 1e-1)
    assert prec.GetXYResolution() == pytest.approx(8.983152841195213e-09)
    assert prec.GetZResolution() == 1e-3
    assert prec.GetMResolution() == 1e-1

    srs = osr.SpatialReference()
    srs.SetFromUserInput("EPSG:4269+8228")  # "NAD83 + NAVD88 height (ft)"
    prec.SetFromMeter(srs, 1e-3, 1e-3, 1e-1)
    assert prec.GetXYResolution() == pytest.approx(8.983152841195213e-09)
    assert prec.GetZResolution() == pytest.approx(0.0032808398950131233)
    assert prec.GetMResolution() == 1e-1

    assert prec.GetFormats() is None
    assert prec.GetFormatSpecificOptions("foo") == {}
    with pytest.raises(Exception, match="Received a NULL pointer"):
        prec.GetFormatSpecificOptions(None)
    prec.SetFormatSpecificOptions("my_format", {"key": "value"})
    assert prec.GetFormats() == ["my_format"]
    assert prec.GetFormatSpecificOptions("my_format") == {"key": "value"}


def test_ogr_geomcoordinate_precision_geom_field():

    geom_fld = ogr.GeomFieldDefn("foo")
    assert geom_fld.GetCoordinatePrecision().GetXYResolution() == 0

    prec = ogr.CreateGeomCoordinatePrecision()
    prec.Set(1e-9, 1e-3, 1e-2)
    geom_fld.SetCoordinatePrecision(prec)
    assert geom_fld.GetCoordinatePrecision().GetXYResolution() == 1e-9
    assert geom_fld.GetCoordinatePrecision().GetZResolution() == 1e-3
    assert geom_fld.GetCoordinatePrecision().GetMResolution() == 1e-2

    feature_defn = ogr.FeatureDefn("test")
    feature_defn.AddGeomFieldDefn(geom_fld)
    assert feature_defn.IsSame(feature_defn)

    for xy, z, m in [
        (1e-9 * 10, 1e-3, 1e-2),
        (1e-9, 1e-3 * 10, 1e-2),
        (1e-9, 1e-3, 1e-2 * 10),
    ]:
        geom_fld2 = ogr.GeomFieldDefn("foo")
        prec = ogr.CreateGeomCoordinatePrecision()
        prec.Set(xy, z, m)
        geom_fld2.SetCoordinatePrecision(prec)
        feature_defn2 = ogr.FeatureDefn("test")
        feature_defn2.AddGeomFieldDefn(geom_fld2)
        assert not feature_defn.IsSame(feature_defn2)
