#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  XODR driver testing.
# Author:   Michael Scholz, German Aerospace Center (DLR)
#           Gülsen Bardak, German Aerospace Center (DLR)
#
###############################################################################
# Copyright 2024 German Aerospace Center (DLR), Institute of Transportation Systems
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################
import gdaltest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("XODR")
xodr_file = "data/xodr/5g_living_lab_A39_Wolfsburg-West.xodr"


def test_ogr_xodr_test_ogrsf():

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro " + xodr_file
    )

    assert "INFO" in ret
    assert "ERROR" not in ret
    assert "FAILURE" not in ret


def test_ogr_xodr_basics():
    """Test basic capabilities:
    - Data source
    - Layer count
    """
    ds = gdal.OpenEx(xodr_file, gdal.OF_VECTOR)
    assert ds is not None, f"Cannot open dataset for file: {xodr_file}"
    assert ds.GetLayerCount() == 6, f"Bad layer count for file: {xodr_file}"


def test_ogr_xodr_undissolvable_layers():
    """Test all point and linestring layers for:
    - Correct feature type definitions
    - Spatial reference system
    """
    ds = gdal.OpenEx(xodr_file, gdal.OF_VECTOR)

    layer_reference_line = ds.GetLayer("ReferenceLine")
    check_feat_def_reference_line(layer_reference_line)
    check_spatial_ref(layer_reference_line)

    layer_lane_border = ds.GetLayer("LaneBorder")
    check_feat_def_lane_border(layer_lane_border)
    check_spatial_ref(layer_lane_border)

    layer_road_object = ds.GetLayer("RoadObject")
    check_feat_def_road_object(layer_road_object)
    check_spatial_ref(layer_road_object)


@pytest.mark.parametrize("dissolve_tin", [True, False])
def test_ogr_xodr_dissolvable_layers(dissolve_tin: bool):
    """Test all TIN layers for:
        - Correct feature type definitions
        - Spatial reference system

    Args:
        dissolve_tin (bool): True if to dissolve triangulated surfaces.
    """
    options = ["DISSOLVE_TIN=" + str(dissolve_tin)]
    ds = gdal.OpenEx(xodr_file, gdal.OF_VECTOR, open_options=options)

    layer_road_mark = ds.GetLayer("RoadMark")
    check_feat_def_road_mark(layer_road_mark, dissolve_tin)
    check_spatial_ref(layer_road_mark)

    layer_lane = ds.GetLayer("Lane")
    check_feat_def_lane(layer_lane, dissolve_tin)
    check_spatial_ref(layer_lane)

    layer_road_signal = ds.GetLayer("RoadSignal")
    check_feat_def_road_signal(layer_road_signal, dissolve_tin)
    check_spatial_ref(layer_road_signal)


def check_feat_def_reference_line(layer):
    assert (
        layer.GetGeomType() == ogr.wkbLineString25D
    ), "bad layer geometry type for ReferenceLine"
    assert layer.GetFeatureCount() == 41
    assert layer.GetLayerDefn().GetFieldCount() == 3
    assert (
        layer.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTReal
        and layer.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString
    )


def check_feat_def_lane_border(layer):
    assert (
        layer.GetGeomType() == ogr.wkbLineString25D
    ), "bad layer geometry type for LaneBorder"
    assert layer.GetFeatureCount() == 230
    assert layer.GetLayerDefn().GetFieldCount() == 5
    assert (
        layer.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
        and layer.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTInteger
        and layer.GetLayerDefn().GetFieldDefn(4).GetType() == ogr.OFTInteger
    )


def check_feat_def_road_mark(layer, dissolve_tin: bool):
    if not dissolve_tin:
        assert (
            layer.GetGeomType() == ogr.wkbTINZ
        ), "bad layer geometry type for RoadMark"
    else:
        assert (
            layer.GetGeomType() == ogr.wkbPolygon25D
        ), "bad layer geometry type for dissolved RoadMark"
    assert layer.GetFeatureCount() == 424
    assert layer.GetLayerDefn().GetFieldCount() == 3
    assert (
        layer.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTInteger
        and layer.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString
    )


def check_feat_def_road_object(layer):
    assert layer.GetGeomType() == ogr.wkbTINZ, "bad layer geometry type for RoadObject"
    assert layer.GetFeatureCount() == 273
    assert layer.GetLayerDefn().GetFieldCount() == 4
    assert (
        layer.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTString
    )


def check_feat_def_lane(layer, dissolve_tin: bool):
    if not dissolve_tin:
        assert layer.GetGeomType() == ogr.wkbTINZ, "bad layer geometry type for Lane"
    else:
        assert (
            layer.GetGeomType() == ogr.wkbPolygon25D
        ), "bad layer geometry type for dissolved Lane"
    assert layer.GetFeatureCount() == 174
    assert layer.GetLayerDefn().GetFieldCount() == 5
    assert (
        layer.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
        and layer.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTInteger
        and layer.GetLayerDefn().GetFieldDefn(4).GetType() == ogr.OFTInteger
    )


def check_feat_def_road_signal(layer, dissolve_tin: bool):
    if not dissolve_tin:
        assert (
            layer.GetGeomType() == ogr.wkbTINZ
        ), "bad layer geometry type for RoadSignal"
    else:
        assert (
            layer.GetGeomType() == ogr.wkbPoint25D
        ), "bad layer geometry type for dissolved RoadSignal"
    assert layer.GetFeatureCount() == 50
    assert layer.GetLayerDefn().GetFieldCount() == 10
    assert (
        layer.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTString
        and layer.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTString
    )


def check_spatial_ref(layer):
    srs_proj4 = layer.GetSpatialRef().ExportToProj4()
    expected_proj4 = (
        "+proj=utm +zone=32 +ellps=GRS80 +towgs84=0,0,0,0,0,0,0 +units=m +no_defs"
    )
    assert srs_proj4 == expected_proj4, "bad spatial reference system"


@pytest.mark.parametrize("eps", [1.0, 0.1])
def test_ogr_xodr_geometry_eps(eps: float):
    """Test correct geometry creation for different values of open option EPS.

    Args:
        eps (float): Value for linear approximation of parametric geometries.
    """
    options = ["EPSILON=" + str(eps)]
    ds = gdal.OpenEx(xodr_file, gdal.OF_VECTOR, open_options=options)

    lyr = ds.GetLayer("ReferenceLine")
    ogr_xodr_check_reference_line_geometry_eps(lyr, eps)


def ogr_xodr_check_reference_line_geometry_eps(lyr, eps: float):
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    wkt = feat.GetGeometryRef().ExportToWkt()
    if eps == 1.0:
        assert (
            wkt
            == "LINESTRING (618251.572934302 5809506.96459625 102.378603962182,618254.944363001 5809506.95481165 102.371268481462,618258.290734177 5809506.56065761 102.363999939623)"
        ), f"wrong geometry created for ReferenceLine with EPS {str(eps)}"
    elif eps == 0.1:
        assert (
            wkt
            == "LINESTRING (618251.572934302 5809506.96459625 102.378603962182,618254.944363001 5809506.95481165 102.371268481462,618257.937110798 5809506.62607284 102.364759846201,618258.290734177 5809506.56065761 102.363999939623)"
        ), f"wrong geometry created for ReferenceLine with EPS {str(eps)}"


@pytest.mark.parametrize("dissolve_tin", [True, False])
def test_ogr_xodr_geometry_dissolve(dissolve_tin: bool):
    """Test correct geometry creation for different values of open option DISSOLVE_TIN.

    Args:
        dissolve_tin (bool): True if to dissolve triangulated surfaces.
    """
    options = ["DISSOLVE_TIN=" + str(dissolve_tin)]
    ds = gdal.OpenEx(xodr_file, gdal.OF_VECTOR, open_options=options)

    lyr = ds.GetLayer("Lane")
    ogr_xodr_check_lane_geometry_dissolve(lyr, dissolve_tin)

    lyr = ds.GetLayer("RoadMark")
    ogr_xodr_check_road_mark_geometry_dissolve(lyr, dissolve_tin)

    lyr = ds.GetLayer("RoadSignal")
    ogr_xodr_check_road_signal_geometry_dissolve(lyr, dissolve_tin)


def ogr_xodr_check_lane_geometry_dissolve(lyr, dissolve_tin: bool):
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    wkt = feat.GetGeometryRef().ExportToWkt()
    if not dissolve_tin:
        assert (
            wkt
            == "TIN Z (((618251.708293914 5809503.30115552 102.206436434521,618253.406110685 5809502.59383908 102.162274831603,618253.40871869 5809503.08668632 102.186041767762,618251.708293914 5809503.30115552 102.206436434521)),((618251.708293914 5809503.30115552 102.206436434521,618251.726901715 5809502.7975446 102.182768671482,618253.406110685 5809502.59383908 102.162274831603,618251.708293914 5809503.30115552 102.206436434521)),((618253.40871869 5809503.08668632 102.186041767762,618254.710111278 5809502.39980074 102.146632509166,618254.735144074 5809502.88656198 102.170637739305,618253.40871869 5809503.08668632 102.186041767762)),((618253.40871869 5809503.08668632 102.186041767762,618253.406110685 5809502.59383908 102.162274831603,618254.710111278 5809502.39980074 102.146632509166,618253.40871869 5809503.08668632 102.186041767762)),((618254.735144074 5809502.88656198 102.170637739305,618256.354637481 5809502.1051039 102.128452978327,618256.414547031 5809502.56472816 102.151918900654,618254.735144074 5809502.88656198 102.170637739305)),((618254.735144074 5809502.88656198 102.170637739305,618254.710111278 5809502.39980074 102.146632509166,618256.354637481 5809502.1051039 102.128452978327,618254.735144074 5809502.88656198 102.170637739305)),((618256.414547031 5809502.56472816 102.151918900654,618257.381896193 5809501.87667676 102.118091279345,618257.465586929 5809502.30800315 102.140735883984,618256.414547031 5809502.56472816 102.151918900654)),((618256.414547031 5809502.56472816 102.151918900654,618256.354637481 5809502.1051039 102.128452978327,618257.381896193 5809501.87667676 102.118091279345,618256.414547031 5809502.56472816 102.151918900654)))"
        ), "wrong geometry created for Lane"
    else:
        assert (
            wkt
            == "POLYGON ((618257.381896193 5809501.87667676 102.118091279345,618256.354637481 5809502.1051039 102.128452978327,618254.710111278 5809502.39980074 102.146632509166,618253.406110685 5809502.59383908 102.162274831603,618251.726901715 5809502.7975446 102.182768671482,618251.708293914 5809503.30115552 102.206436434521,618253.40871869 5809503.08668632 102.186041767762,618254.735144074 5809502.88656198 102.170637739305,618256.414547031 5809502.56472816 102.151918900654,618257.465586929 5809502.30800315 102.140735883984,618257.381896193 5809501.87667676 102.118091279345))"
        ), "wrong geometry created for dissolved Lane"


def ogr_xodr_check_road_mark_geometry_dissolve(lyr, dissolve_tin: bool):
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    wkt = feat.GetGeometryRef().ExportToWkt()
    if not dissolve_tin:
        assert (
            wkt
            == "TIN Z (((618251.72468874 5809502.85743767 102.185583413892,618252.578130818 5809502.64753279 102.169882217474,618252.576002918 5809502.76737822 102.175586986359,618251.72468874 5809502.85743767 102.185583413892)),((618251.72468874 5809502.85743767 102.185583413892,618251.72911469 5809502.73765153 102.179953929071,618252.578130818 5809502.64753279 102.169882217474,618251.72468874 5809502.85743767 102.185583413892)),((618252.576002918 5809502.76737822 102.175586986359,618253.405793556 5809502.53390956 102.159384806253,618253.406427815 5809502.6537686 102.165164856953,618252.576002918 5809502.76737822 102.175586986359)),((618252.576002918 5809502.76737822 102.175586986359,618252.578130818 5809502.64753279 102.169882217474,618253.405793556 5809502.53390956 102.159384806253,618252.576002918 5809502.76737822 102.175586986359)),((618253.406427815 5809502.6537686 102.165164856953,618253.747583384 5809502.4836466 102.15508610511,618253.749521849 5809502.6034901 102.160897877637,618253.406427815 5809502.6537686 102.165164856953)),((618253.406427815 5809502.6537686 102.165164856953,618253.405793556 5809502.53390956 102.159384806253,618253.747583384 5809502.4836466 102.15508610511,618253.406427815 5809502.6537686 102.165164856953)),((618253.749521849 5809502.6034901 102.160897877637,618254.085085834 5809502.43409623 102.150979368988,618254.088411764 5809502.55390772 102.156822862935,618253.749521849 5809502.6034901 102.160897877637)),((618253.749521849 5809502.6034901 102.160897877637,618253.747583384 5809502.4836466 102.15508610511,618254.085085834 5809502.43409623 102.150979368988,618253.749521849 5809502.6034901 102.160897877637)),((618254.088411764 5809502.55390772 102.156822862935,618254.707033446 5809502.33995247 102.143681017939,618254.713189111 5809502.45964901 102.149584000393,618254.088411764 5809502.55390772 102.156822862935)),((618254.088411764 5809502.55390772 102.156822862935,618254.085085834 5809502.43409623 102.150979368988,618254.707033446 5809502.33995247 102.143681017939,618254.088411764 5809502.55390772 102.156822862935)),((618254.713189111 5809502.45964901 102.149584000393,618255.243094449 5809502.25186128 102.137539289407,618255.251990439 5809502.3713828 102.143494733244,618254.713189111 5809502.45964901 102.149584000393)),((618254.713189111 5809502.45964901 102.149584000393,618254.707033446 5809502.33995247 102.143681017939,618255.243094449 5809502.25186128 102.137539289407,618254.713189111 5809502.45964901 102.149584000393)),((618255.251990439 5809502.3713828 102.143494733244,618256.346892323 5809502.04568328 102.125419284058,618256.362382638 5809502.16452451 102.131486672596,618255.251990439 5809502.3713828 102.143494733244)),((618255.251990439 5809502.3713828 102.143494733244,618255.243094449 5809502.25186128 102.137539289407,618256.346892323 5809502.04568328 102.125419284058,618255.251990439 5809502.3713828 102.143494733244)),((618256.362382638 5809502.16452451 102.131486672596,618256.86502563 5809501.93528991 102.120031826125,618256.884079624 5809502.05360925 102.126153745722,618256.362382638 5809502.16452451 102.131486672596)),((618256.362382638 5809502.16452451 102.131486672596,618256.346892323 5809502.04568328 102.125419284058,618256.86502563 5809501.93528991 102.120031826125,618256.362382638 5809502.16452451 102.131486672596)),((618256.884079624 5809502.05360925 102.126153745722,618257.370482622 5809501.81785335 102.11500305465,618257.393309764 5809501.93550017 102.12117950404,618256.884079624 5809502.05360925 102.126153745722)),((618256.884079624 5809502.05360925 102.126153745722,618256.86502563 5809501.93528991 102.120031826125,618257.370482622 5809501.81785335 102.11500305465,618256.884079624 5809502.05360925 102.126153745722)))"
        ), "wrong geometry created for RoadMark"
    else:
        assert (
            wkt
            == "POLYGON ((618253.747583384 5809502.4836466 102.15508610511,618253.405793556 5809502.53390956 102.159384806253,618252.578130818 5809502.64753279 102.169882217474,618251.72911469 5809502.73765153 102.179953929071,618251.72468874 5809502.85743767 102.185583413892,618252.576002918 5809502.76737822 102.175586986359,618253.406427815 5809502.6537686 102.165164856953,618253.749521849 5809502.6034901 102.160897877637,618254.088411764 5809502.55390772 102.156822862935,618254.713189111 5809502.45964901 102.149584000393,618255.251990439 5809502.3713828 102.143494733244,618256.362382638 5809502.16452451 102.131486672596,618256.884079624 5809502.05360925 102.126153745722,618257.393309764 5809501.93550017 102.12117950404,618257.370482622 5809501.81785335 102.11500305465,618256.86502563 5809501.93528991 102.120031826125,618256.346892323 5809502.04568328 102.125419284058,618255.243094449 5809502.25186128 102.137539289407,618254.707033446 5809502.33995247 102.143681017939,618254.085085834 5809502.43409623 102.150979368988,618253.747583384 5809502.4836466 102.15508610511))"
        ), "wrong geometry created for dissolved RoadMark"


def ogr_xodr_check_road_signal_geometry_dissolve(lyr, dissolve_tin: bool):
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    wkt = feat.GetGeometryRef().ExportToWkt()
    if not dissolve_tin:
        assert (
            wkt
            == "TIN Z (((618366.844654328 5809540.96164437 103.568946384872,618366.840967264 5809541.48457345 103.54861591048,618367.044614501 5809540.96290705 103.56516023851,618366.844654328 5809540.96164437 103.568946384872)),((618366.840967264 5809541.48457345 103.54861591048,618367.040927437 5809541.48583613 103.544829764117,618367.044614501 5809540.96290705 103.56516023851,618366.840967264 5809541.48457345 103.54861591048)),((618366.858657359 5809540.99087441 104.318245603892,618367.058617531 5809540.99213709 104.31445945753,618366.854970294 5809541.51380349 104.297915129499,618366.858657359 5809540.99087441 104.318245603892)),((618366.854970294 5809541.51380349 104.297915129499,618367.058617531 5809540.99213709 104.31445945753,618367.054930467 5809541.51506617 104.294128983137,618366.854970294 5809541.51380349 104.297915129499)),((618366.854970294 5809541.51380349 104.297915129499,618367.054930467 5809541.51506617 104.294128983137,618366.840967264 5809541.48457345 103.54861591048,618366.854970294 5809541.51380349 104.297915129499)),((618366.840967264 5809541.48457345 103.54861591048,618367.054930467 5809541.51506617 104.294128983137,618367.040927437 5809541.48583613 103.544829764117,618366.840967264 5809541.48457345 103.54861591048)),((618367.058617531 5809540.99213709 104.31445945753,618366.858657359 5809540.99087441 104.318245603892,618367.044614501 5809540.96290705 103.56516023851,618367.058617531 5809540.99213709 104.31445945753)),((618367.044614501 5809540.96290705 103.56516023851,618366.858657359 5809540.99087441 104.318245603892,618366.844654328 5809540.96164437 103.568946384872,618367.044614501 5809540.96290705 103.56516023851)),((618366.844654328 5809540.96164437 103.568946384872,618366.858657359 5809540.99087441 104.318245603892,618366.854970294 5809541.51380349 104.297915129499,618366.844654328 5809540.96164437 103.568946384872)),((618366.854970294 5809541.51380349 104.297915129499,618366.840967264 5809541.48457345 103.54861591048,618366.844654328 5809540.96164437 103.568946384872,618366.854970294 5809541.51380349 104.297915129499)),((618367.044614501 5809540.96290705 103.56516023851,618367.054930467 5809541.51506617 104.294128983137,618367.058617531 5809540.99213709 104.31445945753,618367.044614501 5809540.96290705 103.56516023851)),((618367.044614501 5809540.96290705 103.56516023851,618367.040927437 5809541.48583613 103.544829764117,618367.054930467 5809541.51506617 104.294128983137,618367.044614501 5809540.96290705 103.56516023851)))"
        ), "wrong geometry created for RoadSignal"
    else:
        assert (
            wkt == "POINT (618366.942790883 5809541.22374025 103.556888074495)"
        ), "wrong geometry created for dissolved RoadSignal"
