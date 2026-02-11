#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR DXF driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import re

import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr, osr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


pytestmark = pytest.mark.require_driver("DXF")

###############################################################################

# Setup the utf-8 string.
sample_text = 'Text Sample1\u00bf\u03bb\n"abc"'
sample_style = 'Text Sample1\u00bf\u03bb\n\\"abc\\"'

###############################################################################
# Check some general things to see if they meet expectations.


def test_ogr_dxf_1():

    ds = ogr.Open("data/dxf/assorted.dxf")

    assert ds is not None

    assert ds.GetLayerCount() == 1, "expected exactly one layer!"

    layer = ds.GetLayer(0)

    assert layer.GetName() == "entities", "did not get expected layer name."
    assert layer.GetDataset().GetDescription() == ds.GetDescription()

    defn = layer.GetLayerDefn()
    assert defn.GetFieldCount() == 6, "did not get expected number of fields."

    fc = layer.GetFeatureCount()
    assert fc == 22, "did not get expected feature count, got %d" % fc


###############################################################################
# Read the first feature, an ellipse and see if it generally meets expectations.


def test_ogr_dxf_2():

    ds = ogr.Open("data/dxf/assorted.dxf")
    layer = ds.GetLayer(0)

    feat = layer.GetNextFeature()

    assert feat.Layer == "0", "did not get expected layer for feature 0"

    assert feat.PaperSpace == None, "did not get expected PaperSpace for feature 0"

    assert feat.GetFID() == 0, "did not get expected fid for feature 0"

    assert (
        feat.SubClasses == "AcDbEntity:AcDbEllipse"
    ), "did not get expected SubClasses on feature 0."

    assert feat.LineType == "ByLayer", "Did not get expected LineType"

    assert feat.EntityHandle == "43", "did not get expected EntityHandle"

    if feat.GetStyleString() != "PEN(c:#000000)":
        print("%s" % feat.GetStyleString())
        pytest.fail("did not get expected style string on feat 0.")

    geom = feat.GetGeometryRef()
    assert (
        geom.GetGeometryType() == ogr.wkbLineString25D
    ), "did not get expected geometry type."

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 1596.12

    assert area >= exp_area - 0.5 and area <= exp_area + 0.5, (
        "envelope area not as expected, got %g." % area
    )

    assert geom.GetX(0) == pytest.approx(73.25, abs=0.001) and geom.GetY(
        0
    ) == pytest.approx(
        139.75, abs=0.001
    ), "first point (%g,%g) not expected location." % (
        geom.GetX(0),
        geom.GetY(0),
    )


###############################################################################
# Second feature should be a partial ellipse.


def test_ogr_dxf_3():

    ds = ogr.Open("data/dxf/assorted.dxf")
    layer = ds.GetLayer(0)
    for _ in range(1):
        layer.GetNextFeature()
    feat = layer.GetNextFeature()

    geom = feat.GetGeometryRef()

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 311.864

    assert area >= exp_area - 0.5 and area <= exp_area + 0.5, (
        "envelope area not as expected, got %g." % area
    )

    assert geom.GetX(0) == pytest.approx(61.133, abs=0.01) and geom.GetY(
        0
    ) == pytest.approx(
        103.592, abs=0.01
    ), "first point (%g,%g) not expected location." % (
        geom.GetX(0),
        geom.GetY(0),
    )


###############################################################################
# Third feature: POINT with RGB true color


def test_ogr_dxf_4():

    ds = ogr.Open("data/dxf/assorted.dxf")
    layer = ds.GetLayer(0)
    for _ in range(2):
        layer.GetNextFeature()
    feat = layer.GetNextFeature()

    ogrtest.check_feature_geometry(feat, "POINT (83.5 160.0 0)")

    assert feat.GetStyleString() == "PEN(c:#ffbeb8)", "got wrong color on POINT"


###############################################################################
# Fourth feature: LINE


def test_ogr_dxf_5():

    ds = ogr.Open("data/dxf/assorted.dxf")
    layer = ds.GetLayer(0)
    for _ in range(3):
        layer.GetNextFeature()
    feat = layer.GetNextFeature()

    ogrtest.check_feature_geometry(feat, "LINESTRING (97.0 159.5 0,108.5 132.25 0)")

    assert (
        feat.GetGeometryRef().GetGeometryType() != ogr.wkbLineString
    ), "not keeping 3D linestring as 3D"


###############################################################################
# Fourth feature: MTEXT


def test_ogr_dxf_6():

    ds = ogr.Open("data/dxf/assorted.dxf")
    layer = ds.GetLayer(0)
    for _ in range(4):
        layer.GetNextFeature()
    feat = layer.GetNextFeature()

    ogrtest.check_feature_geometry(feat, "POINT (84 126)")

    assert (
        feat.GetGeometryRef().GetGeometryType() != ogr.wkbPoint25D
    ), "not keeping 2D text as 2D"

    assert (
        feat.GetStyleString() == 'LABEL(f:"Arial",t:"Test",a:30,s:5g,p:7,c:#000000)'
    ), "got wrong style string"


###############################################################################
# Partial CIRCLE


def test_ogr_dxf_7():

    ds = ogr.Open("data/dxf/assorted.dxf")
    layer = ds.GetLayer(0)
    for _ in range(5):
        layer.GetNextFeature()
    feat = layer.GetNextFeature()

    geom = feat.GetGeometryRef()

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 445.748

    if area < exp_area - 0.5 or area > exp_area + 0.5:
        print(envelope)
        pytest.fail("envelope area not as expected, got %g." % area)

    assert geom.GetX(0) == pytest.approx(115.258, abs=0.01) and geom.GetY(
        0
    ) == pytest.approx(
        107.791, abs=0.01
    ), "first point (%g,%g) not expected location." % (
        geom.GetX(0),
        geom.GetY(0),
    )


###############################################################################
# PaperSpace and dimension


def test_ogr_dxf_8():

    ds = ogr.Open("data/dxf/assorted.dxf")
    layer = ds.GetLayer(0)
    for _ in range(6):
        layer.GetNextFeature()
    # Check that this line is in PaperSpace
    feat = layer.GetNextFeature()

    assert feat.GetField("PaperSpace") == 1, "did not get expected PaperSpace"

    # Dimension lines
    feat = layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    assert (
        geom.GetGeometryType() == ogr.wkbMultiLineString
    ), "did not get expected geometry type."

    ogrtest.check_feature_geometry(
        feat,
        "MULTILINESTRING ((63.8628719444825 149.209935992088,24.3419606685507 111.934531038653),(72.3255686642474 140.237438265109,63.0051995752285 150.119275371538),(32.8046573883157 102.962033311673,23.4842882992968 112.843870418103))",
    )

    # Dimension arrowheads
    feat = layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    assert (
        geom.GetGeometryType() == ogr.wkbPolygon25D
    ), "did not get expected geometry type."

    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((61.7583023958313 147.797704380064 0,63.8628719444825 149.209935992088 0,62.3300839753339 147.191478127097 0,61.7583023958313 147.797704380064 0))",
    )

    feat = layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    assert (
        geom.GetGeometryType() == ogr.wkbPolygon25D
    ), "did not get expected geometry type."

    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((26.4465302172018 113.346762650677 0,24.3419606685507 111.934531038653 0,25.8747486376992 113.952988903644 0,26.4465302172018 113.346762650677 0))",
    )

    # Dimension text
    feat = layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    ogrtest.check_feature_geometry(
        feat, "POINT (42.815907752635709 131.936242584545397)"
    )

    expected_style = 'LABEL(f:"Arial",t:"54.33",p:5,a:43.3,s:2.5g,c:#000000)'
    assert (
        feat.GetStyleString() == expected_style
    ), "Got unexpected style string:\n%s\ninstead of:\n%s" % (
        feat.GetStyleString(),
        expected_style,
    )


###############################################################################
# BLOCK (inlined)


def test_ogr_dxf_9():

    ds = ogr.Open("data/dxf/assorted.dxf")
    layer = ds.GetLayer(0)
    for _ in range(11):
        layer.GetNextFeature()

    # Skip two dimensions each with a line, two arrowheads and text.
    for _ in range(8):
        layer.GetNextFeature()

    # block (merged geometries)
    feat = layer.GetNextFeature()
    geom = feat.GetGeometryRef()

    assert (
        geom.GetGeometryType() == ogr.wkbMultiLineString25D
    ), "did not get expected geometry type."

    ogrtest.check_feature_geometry(
        feat,
        "MULTILINESTRING ((79.069506278985116 121.003652476272777 0,79.716898725419625 118.892590150942851 0),(79.716898725419625 118.892590150942851 0,78.140638855839953 120.440702522851453 0),(78.140638855839953 120.440702522851453 0,80.139111190485622 120.328112532167196 0),(80.139111190485622 120.328112532167196 0,78.619146316248077 118.920737648613908 0),(78.619146316248077 118.920737648613908 0,79.041358781314059 120.975504978601705 0))",
    )

    # First of two MTEXTs
    feat = layer.GetNextFeature()
    assert feat.GetField("Text") == sample_text, "Did not get expected first mtext."

    expected_style = f'LABEL(f:"Arial",t:"{sample_style}",a:45,s:0.5g,p:5,c:#000000)'
    assert (
        feat.GetStyleString() == expected_style
    ), "Got unexpected style string:\n%s\ninstead of:\n%s." % (
        feat.GetStyleString(),
        expected_style,
    )

    ogrtest.check_feature_geometry(
        feat, "POINT (77.602201427662891 120.775897075866169 0)"
    )

    # Second of two MTEXTs
    feat = layer.GetNextFeature()
    assert feat.GetField("Text") == "Second", "Did not get expected second mtext."

    assert (
        feat.GetField("SubClasses") == "AcDbEntity:AcDbMText"
    ), "Did not get expected subclasses."

    ogrtest.check_feature_geometry(
        feat, "POINT (79.977331629005178 119.698291706738644 0)"
    )


###############################################################################
# LWPOLYLINE in an Object Coordinate System.


def test_ogr_dxf_10():

    ocs_ds = ogr.Open("data/dxf/LWPOLYLINE-OCS.dxf")
    ocs_lyr = ocs_ds.GetLayer(0)

    # Skip boring line.
    feat = ocs_lyr.GetNextFeature()

    # LWPOLYLINE in OCS
    feat = ocs_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    assert (
        geom.GetGeometryType() == ogr.wkbLineString25D
    ), "did not get expected geometry type."

    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING (600325.567999998573214 3153021.253000000491738 562.760000000052969,600255.215999998385087 3151973.98600000096485 536.950000000069849,597873.927999997511506 3152247.628000000491738 602.705000000089058)",
    )

    # LWPOLYLINE in OCS with bulge
    feat = ocs_lyr.GetFeature(12)

    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (611415.459819656 3139300.00002682 1807.37309215522,611245.079665823 3139720.59876424 1807.37309215522,611245.079665823 3139720.59876424 1807.37309215522,611244.054791235 3139723.12875936 1807.27984293229,611243.034695086 3139725.64695847 1807.00053001486,611242.024133533 3139728.14162057 1806.53645568869,611241.027818282 3139730.6011144 1805.88978368251,611240.050394615 3139733.01397265 1805.06352907972,611239.096419732 3139735.36894547 1804.06154426071,611238.170341503 3139737.65505289 1802.88850094122,611237.276477734 3139739.86163602 1801.54986839073,611236.418996029 3139741.97840675 1800.0518879321,611235.601894365 3139743.99549572 1798.40154384175,611234.828982446 3139745.90349832 1796.60653078564,611234.103863944 3139747.69351857 1794.67521794327,611233.429919697 3139749.35721058 1792.61660998662,611232.810291944 3139750.88681743 1790.44030509629,611232.247869676 3139752.27520739 1788.15645021029,611231.745275164 3139753.51590716 1785.77569371438,611231.304851737 3139754.60313201 1783.30913579435,611230.928652852 3139755.5318128 1780.76827668182,611230.618432521 3139756.29761959 1778.16496303489,611230.375637135 3139756.89698184 1775.51133270351,611230.201398719 3139757.32710505 1772.81975813727,611230.096529651 3139757.58598378 1770.10278869926,611230.06151888 3139757.67241101 1767.37309215522,611230.06151892 3139757.67241089 1661.18408370228,611230.06151892 3139757.67241089 1661.18408370228,611230.026508154 3139757.75883812 1658.45438717061,611229.921639091 3139758.01771683 1655.73741774404,611229.74740068 3139758.44784002 1653.04584318824,611229.5046053 3139759.04720226 1650.39221286628,611229.194384975 3139759.81300904 1647.78889922769,611228.818186096 3139760.74168982 1645.24804012238,611228.377762675 3139761.82891465 1642.78148220841,611227.87516817 3139763.0696144 1640.40072571739,611227.312745909 3139764.45800435 1638.11687083509,611226.693118163 3139765.98761118 1635.94056594722,611226.019173923 3139767.65130317 1633.88195799181,611225.294055428 3139769.4413234 1631.95064514943,611224.521143516 3139771.34932599 1630.15563209209,611223.704041858 3139773.36641494 1628.50528799927,611222.84656016 3139775.48318565 1627.00730753696,611221.952696397 3139777.68976876 1625.66867498157,611221.026618175 3139779.97587617 1624.49563165602,611220.072643298 3139782.33084897 1623.49364682979,611219.095219637 3139784.74370721 1622.66739221866,611218.098904392 3139787.20320102 1622.02072020306,611217.088342845 3139789.69786311 1621.55664586644,"
        + "611216.0682467 3139792.21606221 1621.27733293758,611215.043372117 3139794.74605732 1621.18408370228,610905.973331759 3140557.71325641 1621.18408370228,610905.973331759 3140557.71325641 1621.18408370228,610904.948457176 3140560.24325151 1621.2773329396,610903.928361033 3140562.76145061 1621.55664587034,610902.917799487 3140565.2561127 1622.02072020868,610901.921484243 3140567.71560651 1622.66739222582,610900.944060583 3140570.12846474 1623.49364683831,610899.990085707 3140572.48343755 1624.49563166573,610899.064007486 3140574.76954495 1625.66867499227,610898.170143725 3140576.97612806 1627.00730754846,610897.312662028 3140579.09289877 1628.50528801138,610896.495560372 3140581.10998771 1630.1556321046,610895.722648461 3140583.0179903 1631.95064516215,610894.997529967 3140584.80801053 1633.88195800453,610894.323585729 3140586.47170251 1635.94056595974,610893.703957984 3140588.00130935 1638.1168708472,610893.141535724 3140589.38969929 1640.4007257289,610892.63894122 3140590.63039904 1642.78148221912,610892.198517801 3140591.71762387 1645.2480401321,610891.822318923 3140592.64630464 1647.78889923622,610891.5120986 3140593.41211142 1650.39221287345,610891.269303221 3140594.01147366 1653.04584319386,610891.095064811 3140594.44159685 1655.73741774794,610890.99019575 3140594.70047556 1658.45438717264,610890.955184986 3140594.78690278 1661.18408370228,610890.955185021 3140594.78690272 1752.31638281001,610890.955185021 3140594.78690271 1752.31638281001,610890.920174252 3140594.87332995 1755.04607934987,610890.815305187 3140595.13220867 1757.76304878401,610890.641066773 3140595.56233187 1760.45462334672,610890.398271389 3140596.16169412 1763.10825367492,610890.088051061 3140596.92750091 1765.71156731903,610889.711852178 3140597.85618169 1768.25242642912,610889.271428753 3140598.94340654 1770.71898434711,610888.768834244 3140600.1841063 1773.09974084137,610888.206411978 3140601.57249626 1775.38359572612,610887.586784228 3140603.1021031 1777.55990061562,610886.912839984 3140604.7657951 1779.61850857185,610886.187721485 3140606.55581535 1781.54982141423,610885.414809569 3140608.46381795 1783.34483447076,610884.597707907 3140610.48090691 1784.99517856195,610883.740226205 3140612.59767763 1786.49315902182,610882.846362438 3140614.80426075 1787.83179157397,610881.920284211 3140617.09036817 1789.0048348955,610880.96630933 3140619.44534098 1790.00681971696,610879.988885665 3140621.85819923 1790.83307432256,610878.992570417 3140624.31769305 1791.47974633192,610877.982008866 3140626.81235515 1791.94382066162,610876.961912718 3140629.33055426 1792.22313358291,610875.937038132 3140631.86054938 1792.31638281001,610699.99993399 3141066.17711854 1792.31638281001)",
    )

    ocs_lyr = None
    ocs_ds = None


###############################################################################
# Test reading from an entities-only dxf file (#3412)


def test_ogr_dxf_11():

    eo_ds = ogr.Open("data/dxf/entities_only.dxf")
    eo_lyr = eo_ds.GetLayer(0)

    # Check first point.
    feat = eo_lyr.GetNextFeature()

    ogrtest.check_feature_geometry(feat, "POINT (672500.0 242000.0 539.986)")

    # Check second point.
    feat = eo_lyr.GetNextFeature()

    ogrtest.check_feature_geometry(feat, "POINT (672750.0 242000.0 558.974)")

    eo_lyr = None
    eo_ds = None


###############################################################################
# Write a simple file with a few geometries of different types, and read back.


def test_ogr_dxf_12(tmp_path):

    ds = ogr.GetDriverByName("DXF").CreateDataSource(tmp_path / "dxf_11.dxf")

    lyr = ds.CreateLayer("entities")
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING(10 12, 60 65)"))
    dst_feat.SetFID(0)
    lyr.CreateFeature(dst_feat)
    # 80 is the minimum handle value we set in case of inapproriate or unset
    # initial FID value
    assert dst_feat.GetFID() >= 80
    dst_feat = None

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("POLYGON((0 0,100 0,100 100,0 0))")
    )
    dst_feat.SetFID(79)
    lyr.CreateFeature(dst_feat)
    assert dst_feat.GetFID() == 79
    dst_feat = None

    # Test 25D linestring with constant Z (#5210)
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING(1 2 10,3 4 10)"))
    dst_feat.SetFID(79)
    lyr.CreateFeature(dst_feat)
    assert dst_feat.GetFID() > 79
    dst_feat = None

    # Test 25D linestring with different Z (#5210)
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("LINESTRING(1 2 -10,3 4 10)")
    )
    lyr.CreateFeature(dst_feat)
    dst_feat = None

    # Test multipoint
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("MULTIPOINT((10 40),(40 30))")
    )
    lyr.CreateFeature(dst_feat)
    dst_feat = None

    lyr = None
    ds = None

    # Read back.
    ds = ogr.Open(tmp_path / "dxf_11.dxf")
    lyr = ds.GetLayer(0)

    # Check first feature
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat, "LINESTRING(10 12, 60 65)"
    ), feat.GetGeometryRef().ExportToWkt()

    assert (
        feat.GetGeometryRef().GetGeometryType() == ogr.wkbLineString
    ), "not linestring 2D"
    feat = None

    # Check second feature
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat, "POLYGON((0 0,100 0,100 100,0 0))"
    ), feat.GetGeometryRef().ExportToWkt()

    assert (
        feat.GetGeometryRef().GetGeometryType() == ogr.wkbPolygon
    ), "not keeping polygon 2D"
    feat = None

    # Check third feature
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat, "LINESTRING(1 2 10,3 4 10)"
    ), feat.GetGeometryRef().ExportToWkt()
    feat = None

    # Check fourth feature
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat, "LINESTRING(1 2 -10,3 4 10)"
    ), feat.GetGeometryRef().ExportToWkt()
    feat = None

    # Check 5th feature (1st multipoint part)
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat, "POINT(10 40)"
    ), feat.GetGeometryRef().ExportToWkt()
    feat = None

    # Check 6th feature (2nd multipoint part)
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat, "POINT(40 30)"
    ), feat.GetGeometryRef().ExportToWkt()
    feat = None

    lyr = None
    ds = None


###############################################################################
# Check smoothed polyline.


def test_ogr_dxf_13():

    ds = ogr.Open("data/dxf/polyline_smooth.dxf")

    layer = ds.GetLayer(0)

    feat = layer.GetNextFeature()

    assert feat.Layer == "1", "did not get expected layer for feature 0"

    geom = feat.GetGeometryRef()
    assert (
        geom.GetGeometryType() == ogr.wkbLineString25D
    ), "did not get expected geometry type."

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 1350.43

    assert area >= exp_area - 0.5 and area <= exp_area + 0.5, (
        "envelope area not as expected, got %g." % area
    )

    # Check for specific number of points from tessellated arc(s).
    # Note that this number depends on the tessellation algorithm and
    # possibly the default global arc_stepsize variable; therefore it is
    # not guaranteed to remain constant even if the input DXF file is constant.
    # If you retain this test, you may need to update the point count if
    # changes are made to the aforementioned items. Ideally, one would test
    # only that more points are returned than in the original polyline, and
    # that the points lie along (or reasonably close to) said path.

    assert geom.GetPointCount() == 146, (
        "did not get expected number of points, got %d" % geom.GetPointCount()
    )

    assert geom.GetX(0) == pytest.approx(251297.8179, abs=0.001) and geom.GetY(
        0
    ) == pytest.approx(
        412226.8286, abs=0.001
    ), "first point (%g,%g) not expected location." % (
        geom.GetX(0),
        geom.GetY(0),
    )

    # Other possible tests:
    # Polylines with no explicit Z coordinates (e.g., no attribute 38 for
    # LWPOLYLINE and no attribute 30 for POLYLINE) should always return
    # geometry type ogr.wkbPolygon. Otherwise, ogr.wkbPolygon25D should be
    # returned even if the Z coordinate values are zero.
    # If the arc_stepsize global is used, one could test that returned adjacent
    # points do not slope-diverge greater than that value.

    ds = None


###############################################################################
# Check smooth LWPOLYLINE entity.


def test_ogr_dxf_14():

    # This test is identical to the previous one except the
    # newer lwpolyline entity is used. See the comments in the
    # previous test regarding caveats, etc.

    ds = ogr.Open("data/dxf/lwpolyline_smooth.dxf")

    layer = ds.GetLayer(0)

    feat = layer.GetNextFeature()

    assert feat.Layer == "1", "did not get expected layer for feature 0"

    geom = feat.GetGeometryRef()
    assert (
        geom.GetGeometryType() == ogr.wkbLineString
    ), "did not get expected geometry type."

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 1350.43

    assert area >= exp_area - 0.5 and area <= exp_area + 0.5, (
        "envelope area not as expected, got %g." % area
    )

    assert geom.GetPointCount() == 146, (
        "did not get expected number of points, got %d" % geom.GetPointCount()
    )

    assert geom.GetX(0) == pytest.approx(251297.8179, abs=0.001) and geom.GetY(
        0
    ) == pytest.approx(
        412226.8286, abs=0.001
    ), "first point (%g,%g) not expected location." % (
        geom.GetX(0),
        geom.GetY(0),
    )

    ds = None


###############################################################################
# Write a file with dynamic layer creation and confirm that the
# dynamically created layer 'abc' matches the definition of the default
# layer '0'.


def test_ogr_dxf_15(tmp_path):

    ds = ogr.GetDriverByName("DXF").CreateDataSource(
        tmp_path / "dxf_14.dxf", ["FIRST_ENTITY=80"]
    )

    lyr = ds.CreateLayer("entities")

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING(10 12, 60 65)"))
    dst_feat.SetField("Layer", "abc")
    lyr.CreateFeature(dst_feat)

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("POLYGON((0 0,100 0,100 100,0 0))")
    )
    lyr.CreateFeature(dst_feat)

    lyr = None
    ds = None

    # Read back.
    ds = ogr.Open(tmp_path / "dxf_14.dxf")
    lyr = ds.GetLayer(0)

    # Check first feature
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat, "LINESTRING(10 12, 60 65)"
    ), feat.GetGeometryRef().ExportToWkt()

    assert (
        feat.GetGeometryRef().GetGeometryType() != ogr.wkbLineString25D
    ), "not linestring 2D"

    assert feat.GetField("Layer") == "abc", "Did not get expected layer, abc."

    # Check second point.
    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat, "POLYGON((0 0,100 0,100 100,0 0))"
    ), feat.GetGeometryRef().ExportToWkt()

    assert (
        feat.GetGeometryRef().GetGeometryType() != ogr.wkbPolygon25D
    ), "not keeping polygon 2D"

    assert feat.GetField("Layer") == "0", "Did not get expected layer, 0."

    lyr = None
    ds = None

    # Check the DXF file itself to try and ensure that the layer
    # is defined essentially as we expect.  We assume the only thing
    # that will be different is the layer name is 'abc' instead of '0'
    # and the entity id.

    outdxf = open(tmp_path / "dxf_14.dxf").read()
    start_1 = outdxf.find("  0\nLAYER")
    start_2 = outdxf.find("  0\nLAYER", start_1 + 10)

    txt_1 = outdxf[start_1:start_2]
    txt_2 = outdxf[start_2 : start_2 + len(txt_1) + 2]

    abc_off = txt_2.find("abc\n")

    assert (
        txt_2[16:abc_off] + "0" + txt_2[abc_off + 3 :] == txt_1[16:]
    ), "Layer abc does not seem to match layer 0."

    # Check that $HANDSEED was set as expected.
    start_seed = outdxf.find("$HANDSEED")
    handseed = outdxf[start_seed + 10 + 4 : start_seed + 10 + 4 + 8]
    assert handseed == "00000053", "Did not get expected HANDSEED, got %s." % handseed


###############################################################################
# Test that ENCODING open option works


def test_ogr_dxf_ENCODING_open_option():

    with gdal.OpenEx("data/dxf/utf-8.dxf", open_options={"ENCODING": "UTF-8"}) as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["Layer"] == "\u00e9ven"


###############################################################################
# Test reading without DXF blocks inlined.


def test_ogr_dxf_16():

    with gdal.config_option("DXF_INLINE_BLOCKS", "FALSE"):

        dxf_ds = ogr.Open("data/dxf/assorted.dxf")

        assert dxf_ds is not None

        assert dxf_ds.GetLayerCount() == 2, "expected exactly two layers!"

        dxf_layer = dxf_ds.GetLayer(1)

        assert dxf_layer.GetName() == "entities", "did not get expected layer name."

        # read through till we encounter the block reference.
        feat = dxf_layer.GetNextFeature()
        while feat.GetField("EntityHandle") != "55":
            feat = dxf_layer.GetNextFeature()

        # check contents.
        assert feat.GetField("BlockName") == "STAR", "Did not get blockname!"

        assert feat.GetField("BlockAngle") == 0.0, "Did not get expected angle."

        assert feat.GetField("BlockScale") == [
            1.0,
            1.0,
            1.0,
        ], "Did not get expected BlockScale"

        ogrtest.check_feature_geometry(
            feat, "POINT (79.097653776656188 119.962195062443342 0)"
        )

        feat = None

        # Now we need to check the blocks layer and ensure it is as expected.

        dxf_layer = dxf_ds.GetLayer(0)

        assert dxf_layer.GetName() == "blocks", "did not get expected layer name."

        # STAR geometry
        feat = dxf_layer.GetNextFeature()

        assert feat.GetField("Block") == "STAR", "Did not get expected block name."

        ogrtest.check_feature_geometry(
            feat,
            "MULTILINESTRING ((-0.028147497671066 1.041457413829428 0,0.619244948763444 -1.069604911500494 0),(0.619244948763444 -1.069604911500494 0,-0.957014920816232 0.478507460408116 0),(-0.957014920816232 0.478507460408116 0,1.041457413829428 0.365917469723853 0),(1.041457413829428 0.365917469723853 0,-0.478507460408116 -1.041457413829428 0),(-0.478507460408116 -1.041457413829428 0,-0.056294995342131 1.013309916158363 0))",
        )

        # First MTEXT
        feat = dxf_layer.GetNextFeature()
        assert feat.GetField("Text") == sample_text, "Did not get expected first mtext."

        expected_style = (
            f'LABEL(f:"Arial",t:"{sample_style}",a:45,s:0.5g,p:5,c:#000000)'
        )
        assert (
            feat.GetStyleString() == expected_style
        ), "Got unexpected style string:\n%s\ninstead of:\n%s." % (
            feat.GetStyleString(),
            expected_style,
        )

        ogrtest.check_feature_geometry(
            feat, "POINT (-1.495452348993292 0.813702013422821 0)"
        )

        # Second MTEXT
        feat = dxf_layer.GetNextFeature()
        assert feat.GetField("Text") == "Second", "Did not get expected second mtext."

        assert (
            feat.GetField("SubClasses") == "AcDbEntity:AcDbMText"
        ), "Did not get expected subclasses."

        ogrtest.check_feature_geometry(
            feat, "POINT (0.879677852348995 -0.263903355704699 0)"
        )

        feat = None


###############################################################################
# Write a file with blocks defined from a source blocks layer.


def test_ogr_dxf_17(tmp_path):

    ds = ogr.GetDriverByName("DXF").CreateDataSource(
        tmp_path / "dxf_17.dxf", ["HEADER=data/dxf/header_extended.dxf"]
    )

    blyr = ds.CreateLayer("blocks")
    lyr = ds.CreateLayer("entities")

    dst_feat = ogr.Feature(feature_def=blyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION( LINESTRING(0 0,1 1),LINESTRING(1 0,0 1))"
        )
    )
    dst_feat.SetField("Block", "XMark")
    blyr.CreateFeature(dst_feat)

    # Block with 2 polygons
    dst_feat = ogr.Feature(feature_def=blyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt(
            "GEOMETRYCOLLECTION( POLYGON((10 10,10 20,20 20,20 10,10 10)),POLYGON((10 -10,10 -20,20 -20,20 -10,10 -10)))"
        )
    )
    dst_feat.SetField("Block", "Block2")
    blyr.CreateFeature(dst_feat)

    # Block with point and line
    dst_feat = ogr.Feature(feature_def=blyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("GEOMETRYCOLLECTION( POINT(1 2),LINESTRING(0 0,1 1))")
    )
    dst_feat.SetField("Block", "Block3")
    blyr.CreateFeature(dst_feat)

    # Write a block reference feature.
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(200 100)"))
    dst_feat.SetField("Layer", "abc")
    dst_feat.SetField("BlockName", "XMark")
    lyr.CreateFeature(dst_feat)

    # Write a block reference feature for a non-existent block.
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(300 50)"))
    dst_feat.SetField("Layer", "abc")
    dst_feat.SetField("BlockName", "DoesNotExist")
    lyr.CreateFeature(dst_feat)

    # Write a block reference feature for a template defined block
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(250 200)"))
    dst_feat.SetField("Layer", "abc")
    dst_feat.SetField("BlockName", "STAR")
    lyr.CreateFeature(dst_feat)

    # Write a block reference feature with scaling and rotation
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(300 100)"))
    dst_feat.SetField("BlockName", "XMark")
    dst_feat.SetField("BlockAngle", "30")
    dst_feat.SetFieldDoubleList(
        lyr.GetLayerDefn().GetFieldIndex("BlockScale"), [4.0, 5.0, 6.0]
    )
    lyr.CreateFeature(dst_feat)

    # Write a Block2 reference feature.
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(350 100)"))
    dst_feat.SetField("Layer", "abc")
    dst_feat.SetField("BlockName", "Block2")
    lyr.CreateFeature(dst_feat)

    # Write a Block3 reference feature.
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(400 100)"))
    dst_feat.SetField("Layer", "abc")
    dst_feat.SetField("BlockName", "Block3")
    lyr.CreateFeature(dst_feat)

    ds = None

    # Reopen and check contents.

    ds = ogr.Open(tmp_path / "dxf_17.dxf")

    lyr = ds.GetLayer(0)

    # Check first feature.
    feat = lyr.GetNextFeature()
    assert (
        feat.GetField("SubClasses") == "AcDbEntity:AcDbBlockReference"
    ), "Got wrong subclasses for feature 1."

    ogrtest.check_feature_geometry(
        feat, "MULTILINESTRING ((200 100,201 101),(201 100,200 101))"
    ), "Feature 1"

    # Check 2nd feature.
    feat = lyr.GetNextFeature()
    assert (
        feat.GetField("SubClasses") == "AcDbEntity:AcDbPoint"
    ), "Got wrong subclasses for feature 2."

    ogrtest.check_feature_geometry(feat, "POINT (300 50)"), "Feature 2"

    # Check 3rd feature.
    feat = lyr.GetNextFeature()
    assert (
        feat.GetField("SubClasses") == "AcDbEntity:AcDbBlockReference"
    ), "Got wrong subclasses for feature 3."

    ogrtest.check_feature_geometry(
        feat,
        "MULTILINESTRING ((249.971852502328943 201.04145741382942 0,250.619244948763452 198.930395088499495 0),(250.619244948763452 198.930395088499495 0,249.042985079183779 200.47850746040811 0),(249.042985079183779 200.47850746040811 0,251.04145741382942 200.365917469723854 0),(251.04145741382942 200.365917469723854 0,249.52149253959189 198.95854258617058 0),(249.52149253959189 198.95854258617058 0,249.943705004657858 201.013309916158363 0))",
    ), "Feature 3"

    # Check 4th feature (scaled and rotated)
    feat = lyr.GetNextFeature()
    assert (
        feat.GetField("SubClasses") == "AcDbEntity:AcDbBlockReference"
    ), "Got wrong subclasses for feature 4."

    ogrtest.check_feature_geometry(
        feat,
        "MULTILINESTRING ((300 100,300.964101615137736 106.330127018922198), (303.464101615137736 102.0,297.5 104.330127018922198))",
    ), "Feature 4"

    # Check 5th feature
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "MULTIPOLYGON (((360 110,360 120,370 120,370 110,360 110)),((360 90,360 80,370 80,370 90,360 90)))",
    ), "Feature 5"

    # Check 6th feature
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "GEOMETRYCOLLECTION (POINT (401 102),LINESTRING (400 100,401 101))"
    ), "Feature 5"

    # Cleanup

    lyr = None
    ds = None


###############################################################################
# Write a file with line patterns, and make sure corresponding Linetypes are
# created.


def test_ogr_dxf_18(tmp_path):

    ds = ogr.GetDriverByName("DXF").CreateDataSource(
        tmp_path / "dxf_18.dxf", ["HEADER=data/dxf/header_extended.dxf"]
    )

    lyr = ds.CreateLayer("entities")

    # Write a feature with a predefined LTYPE in the header.
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING(0 0,25 25)"))
    dst_feat.SetField("Linetype", "DASHED")
    dst_feat.SetStyleString('PEN(c:#ffff00,w:2g,p:"12.0g 6.0g")')
    lyr.CreateFeature(dst_feat)

    # Write a feature with a named linetype but that isn't predefined in the header.
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING(5 5,30 30)"))
    dst_feat.SetField("Linetype", "DOTTED")
    dst_feat.SetStyleString('PEN(c:#ffff00,w:2g,p:"0.0g 4.0g")')
    lyr.CreateFeature(dst_feat)

    # Write a feature without a linetype name - it will be created.
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING(5 5,40 30)"))
    dst_feat.SetStyleString('PEN(c:#ffff00,w:2g,p:"3.0g 4.0g")')
    lyr.CreateFeature(dst_feat)

    # Write a feature with a linetype proportional to a predefined one.
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING(5 5,40 20)"))
    dst_feat.SetStyleString('PEN(c:#ffff00,w:0.3mm,p:"6.35g 3.0617284g")')
    lyr.CreateFeature(dst_feat)

    # Write a feature with a linetype proportional to an auto-created one.
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("LINESTRING(5 5,40 10)"))
    dst_feat.SetStyleString('PEN(c:#ffff00,w:20px,p:"6.0g 8.0g")')
    lyr.CreateFeature(dst_feat)

    ds = None

    # Reopen and check contents.

    ds = ogr.Open(tmp_path / "dxf_18.dxf")

    lyr = ds.GetLayer(0)

    # Check first feature.
    feat = lyr.GetNextFeature()
    assert feat.GetField("Linetype") == "DASHED", "Got wrong linetype. (1)"

    assert (
        feat.GetStyleString() == 'PEN(c:#ffff00,w:2g,p:"12.7g 6.1234567892g")'
    ), "got wrong style string (1)"

    ogrtest.check_feature_geometry(feat, "LINESTRING (0 0,25 25)")

    # Check second feature.
    feat = lyr.GetNextFeature()
    assert feat.GetField("Linetype") == "DOTTED", "Got wrong linetype. (2)"

    assert (
        feat.GetStyleString() == 'PEN(c:#ffff00,w:2g,p:"0g 4g")'
    ), "got wrong style string (2)"

    ogrtest.check_feature_geometry(feat, "LINESTRING (5 5,30 30)")

    # Check third feature.
    feat = lyr.GetNextFeature()
    assert feat.GetField("Linetype") == "AutoLineType-1", "Got wrong linetype. (3)"

    assert (
        feat.GetStyleString() == 'PEN(c:#ffff00,w:2g,p:"3g 4g")'
    ), "got wrong style string (3)"

    ogrtest.check_feature_geometry(feat, "LINESTRING (5 5,40 30)")

    # Check fourth feature.
    feat = lyr.GetNextFeature()
    assert feat.GetField("Linetype") == "DASHED", "Got wrong linetype. (4)"

    # TODO why did the lineweight go AWOL here?
    assert (
        feat.GetStyleString() == 'PEN(c:#ffff00,p:"6.35g 3.0617283946g")'
    ), "got wrong style string (4)"

    # Check fifth feature.
    feat = lyr.GetNextFeature()
    assert feat.GetField("Linetype") == "AutoLineType-1", "Got wrong linetype. (5)"

    assert (
        feat.GetStyleString() == 'PEN(c:#ffff00,w:0.01g,p:"6g 8g")'
    ), "got wrong style string (5)"

    # Cleanup

    lyr = None
    ds = None


###############################################################################
# Test writing a file using references to blocks defined entirely in the
# template - no blocks layer transferred.


def test_ogr_dxf_19(tmp_path):

    ds = ogr.GetDriverByName("DXF").CreateDataSource(
        tmp_path / "dxf_19.dxf", ["HEADER=data/dxf/header_extended.dxf"]
    )

    lyr = ds.CreateLayer("entities")

    # Write a block reference feature for a template defined block
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT(250 200)"))
    dst_feat.SetField("Layer", "abc")
    dst_feat.SetField("BlockName", "STAR")
    lyr.CreateFeature(dst_feat)

    ds = None

    # Reopen and check contents.

    ds = ogr.Open(tmp_path / "dxf_19.dxf")

    lyr = ds.GetLayer(0)

    # Check first feature.
    feat = lyr.GetNextFeature()
    assert (
        feat.GetField("SubClasses") == "AcDbEntity:AcDbBlockReference"
    ), "Got wrong subclasses for feature 1."

    ogrtest.check_feature_geometry(
        feat,
        "MULTILINESTRING ((249.971852502328943 201.04145741382942 0,250.619244948763452 198.930395088499495 0),(250.619244948763452 198.930395088499495 0,249.042985079183779 200.47850746040811 0),(249.042985079183779 200.47850746040811 0,251.04145741382942 200.365917469723854 0),(251.04145741382942 200.365917469723854 0,249.52149253959189 198.95854258617058 0),(249.52149253959189 198.95854258617058 0,249.943705004657858 201.013309916158363 0))",
    )

    # Cleanup

    lyr = None
    ds = None


###############################################################################
# SPLINE


def test_ogr_dxf_20():

    ds = ogr.Open("data/dxf/spline_qcad.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (10.75 62.75 0,20.6377527691461 63.4348325014897 0,29.2832390843855 63.3968383943819 0,36.7669438145629 62.7115659755966 0,43.1693518285229 61.4545635420541 0,48.5709479951103 59.7013793906746 0,53.0522171831696 57.5275618183782 0,56.6936442615455 55.0086591220851 0,59.5757140990827 52.2202195987154 0,61.7789115646259 49.2377915451895 0,63.3837215270196 46.1369232584274 0,64.4706288551086 42.9931630353494 0,65.1201184177375 39.8820591728755 0,65.4124191318699 36.8783587852151 0,65.4178097850938 34.0256630086877 0,65.1936435950041 31.3271132527085 0,64.7964099415976 28.7831469350429 0,64.2825982048708 26.3942014734563 0,63.7086977648202 24.1607142857143 0,63.1311980014424 22.0831227895822 0,62.6065882947339 20.1618644028256 0,62.1913580246914 18.3973765432099 0,61.9419965713113 16.7900966285005 0,61.9149933145902 15.340462076463 0,62.1668376345247 14.0489103048627 0,62.7540189111114 12.9158787314652 0,63.7236522867034 11.9407009815488 0,65.0535714285714 11.1145529640428 0,66.6905578417924 10.4249542752629 0,68.5812465589803 9.85940726476756 0,70.6722726127488 9.40541428211496 0,72.9102710357119 9.05047767686342 0,75.2418768604836 8.7820997985712 0,77.6137251196775 8.5877829967966 0,79.9724508459076 8.4550296210979 0,82.2646890717878 8.37134202103338 0,84.4370748299319 8.32422254616132 0,86.4362431529539 8.30117354604001 0,88.2089267217763 8.28977110636533 0,89.7225596587841 8.29322337400568 0,90.9907637364175 8.34961568891715 0,92.0334102188788 8.5017525038626 0,92.8703703703704 8.79243827160492 0,93.5215154550944 9.264477444907 0,94.0067167372534 9.9606744765317 0,94.3458454810496 10.9238338192419 0,94.5587729506853 12.1967599258005 0,94.6653704103629 13.8222572489704 0,94.6855091242846 15.8431302415145 0,94.6390603566529 18.3021833561955 0,94.5458953716701 21.2422210457765 0,94.4214717633085 24.7020300183563 0,94.2152055413583 28.6602796174316 0,93.8256737733306 33.0493607201842 0,93.1501457725948 37.8004737609325 0,92.0858908525198 42.8448191739948 0,90.5301783264748 48.1135973936895 0,88.3802775078288 53.5380088543349 0,85.5334577099508 59.0492539902493 0,81.8869882462101 64.5785332357511 0,77.3381384299757 70.0570470251587 0,71.7841775746166 75.4159957927904 0,65.122374993502 80.5865799729645 0,57.25 85.5 0)",
    )
    ds = None


###############################################################################
# CIRCLE and long ARC with specified maximum interval between vertices


def test_ogr_dxf_21():

    ds = ogr.Open("data/dxf/circle.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()

    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING (5 2 3,4.990256201039297 1.720974105023499 3,4.961072274966281 1.443307596159738 3,4.912590402935223 1.168353236728963 3,4.845046783753276 0.897450576732003 3,4.758770483143634 0.631919426697325 3,4.654181830570403 0.373053427696799 3,4.531790371435708 0.122113748856437 3,4.392192384625703 -0.11967705693282 3,4.23606797749979 -0.351141009169893 3,4.064177772475912 -0.571150438746157 3,3.877359201354605 -0.778633481835989 3,3.676522425435433 -0.972579301909577 3,3.462645901302633 -1.152043014426888 3,3.236771613882987 -1.316150290220167 3,3.0 -1.464101615137754 3,2.75348458715631 -1.595176185196668 3,2.498426373663648 -1.70873541826715 3,2.23606797749979 -1.804226065180614 3,1.967687582398672 -1.881182905103986 3,1.694592710667722 -1.939231012048832 3,1.418113853070614 -1.978087581473093 3,1.139597986810004 -1.997563308076383 3,0.860402013189997 -1.997563308076383 3,0.581886146929387 -1.978087581473094 3,0.305407289332279 -1.939231012048832 3,0.032312417601329 -1.881182905103986 3,-0.236067977499789 -1.804226065180615 3,-0.498426373663648 -1.70873541826715 3,-0.75348458715631 -1.595176185196668 3,-1.0 -1.464101615137755 3,-1.236771613882987 -1.316150290220167 3,-1.462645901302633 -1.152043014426888 3,-1.676522425435433 -0.972579301909577 3,-1.877359201354605 -0.778633481835989 3,-2.064177772475912 -0.571150438746158 3,-2.236067977499789 -0.351141009169893 3,-2.392192384625704 -0.11967705693282 3,-2.531790371435707 0.122113748856436 3,-2.654181830570403 0.373053427696798 3,-2.758770483143633 0.631919426697324 3,-2.845046783753275 0.897450576732001 3,-2.912590402935223 1.168353236728963 3,-2.961072274966281 1.443307596159737 3,-2.990256201039297 1.720974105023498 3,-3.0 2.0 3,-2.990256201039297 2.279025894976499 3,-2.961072274966281 2.556692403840262 3,-2.912590402935223 2.831646763271036 3,-2.845046783753276 3.102549423267996 3,-2.758770483143634 3.368080573302675 3,-2.654181830570404 3.626946572303199 3,-2.531790371435708 3.877886251143563 3,-2.392192384625704 4.119677056932819 3,-2.23606797749979 4.351141009169892 3,-2.064177772475912 4.571150438746157 3,-1.877359201354604 4.778633481835989 3,-1.676522425435434 4.972579301909576 3,-1.462645901302632 5.152043014426889 3,-1.236771613882989 5.316150290220166 3,-1.0 5.464101615137753 3,-0.753484587156311 5.595176185196667 3,-0.498426373663649 5.70873541826715 3,-0.23606797749979 5.804226065180615 3,0.032312417601329 5.881182905103985 3,0.305407289332279 5.939231012048833 3,0.581886146929387 5.978087581473094 3,0.860402013189993 5.997563308076383 3,1.139597986810005 5.997563308076383 3,1.418113853070612 5.978087581473094 3,1.69459271066772 5.939231012048833 3,1.96768758239867 5.881182905103986 3,2.236067977499789 5.804226065180615 3,2.498426373663648 5.70873541826715 3,2.75348458715631 5.595176185196668 3,3.0 5.464101615137754 3,3.236771613882985 5.316150290220168 3,3.462645901302634 5.152043014426887 3,3.676522425435431 4.972579301909578 3,3.877359201354603 4.778633481835991 3,4.064177772475912 4.571150438746159 3,4.23606797749979 4.351141009169893 3,4.392192384625702 4.119677056932823 3,4.531790371435708 3.877886251143563 3,4.654181830570404 3.626946572303201 3,4.758770483143634 3.368080573302675 3,4.845046783753275 3.102549423267999 3,4.912590402935223 2.831646763271039 3,4.961072274966281 2.556692403840263 3,4.990256201039298 2.279025894976499 3,5.0 2.0 3)",
    )

    with gdal.config_option("OGR_ARC_MAX_GAP", "80"):
        feat = lyr.GetNextFeature()
        geom = feat.GetGeometryRef()

    assert geom.GetPointCount() == 4, (
        "did not get expected number of points, got %d" % geom.GetPointCount()
    )

    ds = None


###############################################################################
# TEXT


def test_ogr_dxf_22(tmp_vsimem):

    # Read MTEXT feature
    ds = ogr.Open("data/dxf/text.dxf")
    lyr = ds.GetLayer(0)

    test_text = "test\ttext ab/c~d\u00b1ef^g.h#i jklm"

    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString("Text") == test_text, "bad attribute"
    style = (
        'LABEL(f:"SwissCheese",bo:1,t:"' + test_text + '",a:45,s:10g,w:51,c:#ff0000)'
    )
    assert feat.GetStyleString() == style, "bad style"
    ogrtest.check_feature_geometry(feat, "POINT(1 2 3)"), "bad geometry"

    # Write text feature
    out_ds = ogr.GetDriverByName("DXF").CreateDataSource(tmp_vsimem / "ogr_dxf_22.dxf")
    out_lyr = out_ds.CreateLayer("entities")
    out_feat = ogr.Feature(out_lyr.GetLayerDefn())
    out_feat.SetStyleString(style)
    out_feat.SetGeometry(feat.GetGeometryRef())
    out_lyr.CreateFeature(out_feat)
    out_feat = None
    out_lyr = None
    out_ds = None

    ds = None

    # Check written file
    ds = ogr.Open(tmp_vsimem / "ogr_dxf_22.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString("Text") == test_text, "bad attribute"
    assert feat.GetStyleString() == style, "bad style"
    ogrtest.check_feature_geometry(feat, "POINT(1 2 3)"), "bad geometry"

    ds = None

    # Now try reading in the MTEXT feature without translating escape sequences
    with gdal.config_option("DXF_TRANSLATE_ESCAPE_SEQUENCES", "FALSE"):
        ds = ogr.Open("data/dxf/text.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    assert (
        feat.GetFieldAsString("Text")
        == r"\A1;test^Itext\~\pt0.2;{\H0.7x;\Sab\/c\~d%%p^ ef\^ g.h\#i;} j{\L\Ok\ol}m"
    ), "bad attribute with DXF_TRANSLATE_ESCAPE_SEQUENCES = FALSE"

    ds = None


###############################################################################
# POLYGON with hole


def test_ogr_dxf_23(tmp_vsimem):

    # Write polygon
    out_ds = ogr.GetDriverByName("DXF").CreateDataSource(tmp_vsimem / "ogr_dxf_23.dxf")
    out_lyr = out_ds.CreateLayer("entities")
    out_feat = ogr.Feature(out_lyr.GetLayerDefn())
    out_feat.SetStyleString("BRUSH(fc:#ff0000)")
    wkt = "POLYGON ((0 0,0 10,10 10,10 0,0 0),(1 1,1 9,9 9,9 1,1 1))"
    out_feat.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
    out_lyr.CreateFeature(out_feat)
    out_feat = None
    out_lyr = None
    out_ds = None

    ds = None

    # Check written file
    ds = ogr.Open(tmp_vsimem / "ogr_dxf_23.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    style = feat.GetStyleString()
    assert style == "BRUSH(fc:#ff0000)", "bad style"
    ogrtest.check_feature_geometry(feat, wkt), "bad geometry"

    ds = None


###############################################################################
# HATCH


def test_ogr_dxf_24():

    ds = ogr.Open("data/dxf/hatch.dxf")
    lyr = ds.GetLayer(0)

    with gdal.config_option("OGR_ARC_STEPSIZE", "45"):
        feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON ((2 1,1.646446609406726 0.853553390593274,1.5 0.5,1.646446609406726 0.146446609406726,2 0,2.146446609406726 -0.353553390593274,2.5 -0.5,2.853553390593274 -0.353553390593274,3.0 -0.0,3.353553390593274 0.146446609406726,3.5 0.5,3.353553390593274 0.853553390593273,3 1,2.853553390593274 1.353553390593274,2.5 1.5,2.146446609406726 1.353553390593274,2 1))",
    )

    with gdal.config_option("OGR_ARC_STEPSIZE", "45"):
        feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON ((0.0 0.0 0,-0.353553390593274 0.146446609406726 0,-0.5 0.5 0,-0.353553390593274 0.853553390593274 0,-0.0 1.0 0,0.146446609406726 1.353553390593274 0,0.5 1.5 0,0.853553390593274 1.353553390593274 0,1.0 1.0 0,1.353553390593274 0.853553390593274 0,1.5 0.5 0,1.353553390593274 0.146446609406727 0,1.0 0.0 0,0.853553390593274 -0.353553390593274 0,0.5 -0.5 0,0.146446609406726 -0.353553390593274 0,0.0 0.0 0))",
    )

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POLYGON Z ((-1 -1 0,-1 0 0,0 0 0,-1 -1 0))")
    ds = None


###############################################################################
# HATCH as multipolygon


def test_ogr_dxf_hatch_as_multipolygon():

    ds = ogr.Open("data/dxf/hatch_as_multipolygon.dxf")
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "MULTIPOLYGON (((358.626489135389 -3222.17795096722,351.920706594896 -3213.54792929232,337.147774411618 -3225.08824421182,343.594999049021 -3233.57143454121,358.626489135389 -3222.17795096722)),((360.430081693214 -3202.7544412048,349.899032929544 -3210.94613189617,361.299111050642 -3225.61748765378,371.881862297769 -3217.55275382195,360.430081693214 -3202.7544412048)),((385.708838342902 -3180.24073842575,375.124916009045 -3188.5664703005,390.518216036502 -3207.99358484682,400.893711263331 -3199.74547871252,385.708838342902 -3180.24073842575)),((416.621582347477 -3157.76930669891,406.267666436211 -3165.99202157796,417.840195295436 -3180.68678146655,428.155848795364 -3172.42224530735,416.621582347477 -3157.76930669891)),((443.941789741891 -3157.62648135038,472.450365531523 -3135.23997965228,492.417869401406 -3119.31217693865,484.467672055775 -3109.05636151571,442.474089717468 -3142.13601758284,436.789227435105 -3145.3164135512,418.482557291223 -3160.13345804971,426.909413064754 -3170.83879426431,443.941789741891 -3157.62648135038)))",
    )


###############################################################################
# 3DFACE


def test_ogr_dxf_25():

    ds = ogr.Open("data/dxf/3dface.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "POLYGON ((10 20 30,11 21 31,12 22 32,10 20 30))"
    )

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "POLYGON ((10 20 30,11 21 31,12 22 32,13 23 33,10 20 30))"
    )

    ds = None


###############################################################################
# SOLID (#5380)


def test_ogr_dxf_26():

    ds = ogr.Open("data/dxf/solid.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON ((2.716846 2.762514,2.393674 1.647962,4.391042 1.06881,4.714214 2.183362,2.716846 2.762514))",
    )

    ds = None


###############################################################################
# WIPEOUT (#11022)


def test_ogr_dxf_read_wipeout():

    ds = ogr.Open("data/dxf/wipeout.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON ((448381.028869725 6913933.17804321,448381.232017696 6913933.39891582,448380.807997101 6913933.38119118,448381.028869725 6913933.17804321,448381.011145071 6913933.6020638,448381.232017696 6913933.39891582,448381.028869725 6913933.17804321))",
    )

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON ((448380.538954307 6913930.73282502,448380.538954307 6913930.73282502,448380.538954307 6913931.73282502,448381.538954307 6913931.73282502,448381.538954307 6913930.73282502,448380.538954307 6913930.73282502))",
    )


###############################################################################
# Test reading a DXF file without .dxf extensions (#5994)


def test_ogr_dxf_27(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "a_dxf_without_extension", open("data/dxf/solid.dxf").read()
    )

    ds = ogr.Open(tmp_vsimem / "a_dxf_without_extension")
    assert ds is not None


###############################################################################
# Test reading a ELLIPSE with Z extrusion axis value of -1.0 (#5075)


def test_ogr_dxf_28():

    ds = ogr.Open("data/dxf/ellipse_z_extrusion_minus_1.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (247.379588068074 525.677518653024 0,247.560245171261 525.685592896308 0,247.739941456101 525.705876573267 0,247.917852718752 525.738276649788 0,248.09316294264 525.782644518081 0,248.265068041245 525.838776678293 0,248.432779546163 525.90641567189 0,248.595528223532 525.985251262527 0,248.752567602242 526.074921858996 0,248.903177397731 526.175016173715 0,249.046666815684 526.285075109164 0,249.182377720457 526.404593863601 0,249.309687653722 526.533024246411 0,249.428012689458 526.669777192466 0,249.536810112221 526.814225463957 0,249.635580906384 526.965706527318 0,249.723872044951 527.123525592032 0)",
    )

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (290.988651614349 531.01336644407 0,290.900681473157 531.171364661134 0,290.823607338001 531.334954880971 0,290.757782720911 531.503386772611 0,290.703509536322 531.675887798031 0,290.661036716299 531.85166675552 0,290.630559068775 532.029917408641 0,290.612216384031 532.209822184155 0,290.606092793529 532.390555921946 0,290.612216384031 532.571289659737 0,290.630559068775 532.751194435252 0,290.661036716299 532.929445088373 0,290.703509536321 533.105224045862 0,290.75778272091 533.277725071282 0,290.823607338 533.446156962922 0,290.900681473156 533.60974718276 0,290.988651614348 533.767745399824 0)",
    )

    ds = None


###############################################################################
# SPLINE with weights


def test_ogr_dxf_29():

    ds = ogr.Open("data/dxf/spline_weight.dxf")
    lyr = ds.GetLayer(0)

    # spline 227, no weight
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 2 0,2.1025641025641 2.15371128980596 0,2.2051282051282 2.30661339537079 0,2.30769230769231 2.45789713245335 0,2.41025641025641 2.60675331681249 0,2.51282051282051 2.75237276420708 0,2.61538461538461 2.89394629039599 0,2.71794871794872 3.03066471113808 0,2.82051282051282 3.16171884219221 0,2.92307692307692 3.28629949931725 0,3.02564102564102 3.40359749827205 0,3.12820512820513 3.51280365481549 0,3.23076923076923 3.61310878470642 0,3.33333333333333 3.7037037037037 0,3.43589743589744 3.78377922756621 0,3.53846153846154 3.8525261720528 0,3.64102564102564 3.90913535292233 0,3.74358974358974 3.95279758593368 0,3.84615384615384 3.9827036868457 0,3.94871794871795 3.99804447141725 0,4.05128205128205 3.99804447141725 0,4.15384615384615 3.9827036868457 0,4.25641025641026 3.95279758593368 0,4.35897435897436 3.90913535292233 0,4.46153846153846 3.8525261720528 0,4.56410256410256 3.78377922756621 0,4.66666666666667 3.7037037037037 0,4.76923076923077 3.61310878470642 0,4.87179487179487 3.51280365481549 0,4.97435897435897 3.40359749827205 0,5.07692307692308 3.28629949931725 0,5.17948717948718 3.16171884219221 0,5.28205128205128 3.03066471113808 0,5.38461538461539 2.89394629039599 0,5.48717948717949 2.75237276420708 0,5.58974358974359 2.60675331681249 0,5.69230769230769 2.45789713245334 0,5.7948717948718 2.30661339537079 0,5.8974358974359 2.15371128980596 0,6 2 0)",
    )

    # spline 261, weight(3) = 2.0
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 2 0,2.10976572340247 2.16451423293308 0,2.23113089996843 2.34563306806665 0,2.35994706660785 2.5363917071019 0,2.4923982030663 2.7304119838212 0,2.62522401782792 2.92225217784288 0,2.7558282208589 3.10756646216769 0,2.88229155950089 3.2831087138637 0,3.00331913560022 3.44663029387883 0,3.11815132314573 3.5967200894521 0,3.22646181024907 3.73262490129686 0,3.32825875170488 3.85407546924726 0,3.42379815888169 3.96113194681214 0,3.51351351351351 4.05405405405405 0,3.59796229770158 4.13319556415278 0,3.67778836987607 4.19891960597394 0,3.75369845584408 4.25152969753051 0,3.82645109024315 4.29121073470179 0,3.89685688129387 4.31797375648459 0,3.96578985841365 4.33159771032714 0,4.03421014158635 4.33159771032714 0,4.10314311870613 4.31797375648459 0,4.17354890975685 4.29121073470179 0,4.24630154415592 4.25152969753051 0,4.32221163012393 4.19891960597394 0,4.40203770229842 4.13319556415278 0,4.48648648648649 4.05405405405405 0,4.57620184111831 3.96113194681214 0,4.67174124829512 3.85407546924726 0,4.77353818975093 3.73262490129686 0,4.88184867685427 3.59672008945211 0,4.99668086439978 3.44663029387883 0,5.11770844049911 3.2831087138637 0,5.24417177914111 3.10756646216769 0,5.37477598217208 2.92225217784288 0,5.5076017969337 2.7304119838212 0,5.64005293339215 2.53639170710189 0,5.76886910003157 2.34563306806665 0,5.89023427659753 2.16451423293307 0,6 2 0)",
    )

    # spline 262, weight(3) = 0.5
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 2 0,2.09894267472891 2.14827889065297 0,2.1918380517297 2.28667017645161 0,2.28029602220166 2.41674375578168 0,2.36573488380154 2.53972930350242 0,2.44943227756881 2.65657187049601 0,2.53256150506512 2.76796912686927 0,2.6162160195058 2.87439499253976 0,2.70142356019971 2.9761131424117 0,2.78915046059365 3.07318321392016 0,2.88029601503322 3.1654623297623 0,2.97567642514756 3.25260468248623 0,3.07599781301257 3.33406232914161 0,3.18181818181818 3.40909090909091 0,3.29349914490214 3.47676456305555 0,3.41114982578397 3.53600464576074 0,3.53456755043606 3.5856265436821 0,3.66318260330395 3.62440763541344 0,3.79601689800845 3.65117682558841 0,3.93166808931219 3.66492205400063 0,4.06833191068781 3.66492205400063 0,4.20398310199155 3.65117682558841 0,4.33681739669604 3.62440763541344 0,4.46543244956394 3.58562654368211 0,4.58885017421603 3.53600464576074 0,4.70650085509786 3.47676456305555 0,4.81818181818182 3.40909090909091 0,4.92400218698742 3.33406232914161 0,5.02432357485243 3.25260468248623 0,5.11970398496678 3.1654623297623 0,5.21084953940635 3.07318321392016 0,5.29857643980029 2.9761131424117 0,5.3837839804942 2.87439499253976 0,5.46743849493488 2.76796912686927 0,5.55056772243119 2.65657187049601 0,5.63426511619847 2.53972930350242 0,5.71970397779834 2.41674375578168 0,5.8081619482703 2.2866701764516 0,5.90105732527109 2.14827889065297 0,6 2 0)",
    )

    ds = None


###############################################################################
# SPLINE closed


def test_ogr_dxf_30():

    ds = ogr.Open("data/dxf/spline_closed.dxf")
    lyr = ds.GetLayer(0)

    # spline 24b, closed
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (14 2 0,13.9043277090443 2.1111553863727 0,13.8231089120438 2.23728477691191 0,13.7564850364624 2.3759387050853 0,13.7045975097642 2.5246677043605 0,13.6675877594132 2.68102230820516 0,13.6455972128733 2.84255305008692 0,13.6387672976085 3.00681046347343 0,13.6472394410828 3.17134508183234 0,13.6711550707602 3.33370743863128 0,13.7106556141046 3.49144806733791 0,13.7658824985801 3.64211750141987 0,13.8369771516507 3.7832662743448 0,13.9240810007803 3.91244491958036 0,14.0273290209536 4.02721042307341 0,14.1460754371684 4.12590052075793 0,14.2781581418028 4.20836928118745 0,14.4212408102955 4.27464498985476 0,14.5729871180854 4.32475593225266 0,14.7310607406114 4.35873039387394 0,14.8931253533123 4.37659666021141 0,15.0568446316268 4.37838301675786 0,15.2198822509939 4.36411774900609 0,15.3799018868524 4.3338291424489 0,15.5345672146411 4.28754548257907 0,15.6815419097988 4.22529505488942 0,15.8184896477644 4.14710614487273 0,15.9430741039767 4.0530070380218 0,16.0530070380218 3.94307410397669 0,16.1471061448727 3.81848964776439 0,16.2252950548894 3.68154190979879 0,16.2875454825791 3.53456721464106 0,16.3338291424489 3.37990188685238 0,16.3641177490061 3.21988225099391 0,16.3783830167579 3.05684463162681 0,16.3765966602114 2.89312535331225 0,16.3587303938739 2.73106074061141 0,16.3247559322527 2.57298711808543 0,16.2746449898548 2.4212408102955 0,16.2083692811874 2.27815814180278 0,16.1259005207579 2.14607543716844 0,16.0272104230734 2.02732902095363 0,15.9124449195804 1.92408100078027 0,15.7832662743448 1.83697715165069 0,15.6421175014199 1.76588249858013 0,15.4914480673379 1.71065561410461 0,15.3337074386313 1.67115507076015 0,15.1713450818323 1.64723944108276 0,15.0068104634734 1.63876729760846 0,14.8425530500869 1.64559721287326 0,14.6810223082052 1.66758775941317 0,14.5246677043605 1.70459750976422 0,14.3759387050853 1.75648503646241 0,14.2372847769119 1.82310891204376 0,14.1111553863727 1.90432770904428 0,14 2 0)",
    )

    # spline 24c, closed, recalculate knots
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (14 2 0,13.9043277090443 2.1111553863727 0,13.8231089120438 2.23728477691191 0,13.7564850364624 2.3759387050853 0,13.7045975097642 2.5246677043605 0,13.6675877594132 2.68102230820516 0,13.6455972128733 2.84255305008692 0,13.6387672976085 3.00681046347343 0,13.6472394410828 3.17134508183234 0,13.6711550707602 3.33370743863128 0,13.7106556141046 3.49144806733791 0,13.7658824985801 3.64211750141987 0,13.8369771516507 3.7832662743448 0,13.9240810007803 3.91244491958036 0,14.0273290209536 4.02721042307341 0,14.1460754371684 4.12590052075793 0,14.2781581418028 4.20836928118745 0,14.4212408102955 4.27464498985476 0,14.5729871180854 4.32475593225266 0,14.7310607406114 4.35873039387394 0,14.8931253533123 4.37659666021141 0,15.0568446316268 4.37838301675786 0,15.2198822509939 4.36411774900609 0,15.3799018868524 4.3338291424489 0,15.5345672146411 4.28754548257907 0,15.6815419097988 4.22529505488942 0,15.8184896477644 4.14710614487273 0,15.9430741039767 4.0530070380218 0,16.0530070380218 3.94307410397669 0,16.1471061448727 3.81848964776439 0,16.2252950548894 3.68154190979879 0,16.2875454825791 3.53456721464106 0,16.3338291424489 3.37990188685238 0,16.3641177490061 3.21988225099391 0,16.3783830167579 3.05684463162681 0,16.3765966602114 2.89312535331225 0,16.3587303938739 2.73106074061141 0,16.3247559322527 2.57298711808543 0,16.2746449898548 2.4212408102955 0,16.2083692811874 2.27815814180278 0,16.1259005207579 2.14607543716844 0,16.0272104230734 2.02732902095363 0,15.9124449195804 1.92408100078027 0,15.7832662743448 1.83697715165069 0,15.6421175014199 1.76588249858013 0,15.4914480673379 1.71065561410461 0,15.3337074386313 1.67115507076015 0,15.1713450818323 1.64723944108276 0,15.0068104634734 1.63876729760846 0,14.8425530500869 1.64559721287326 0,14.6810223082052 1.66758775941317 0,14.5246677043605 1.70459750976422 0,14.3759387050853 1.75648503646241 0,14.2372847769119 1.82310891204376 0,14.1111553863727 1.90432770904428 0,14 2 0)",
    )

    ds = None


###############################################################################
# OCS2WCS transformations 1


def test_ogr_dxf_31():

    ds = ogr.Open("data/dxf/ocs2wcs1.dxf")
    lyr = ds.GetLayer(0)

    # INFO: Open of `ocs2wcs1.dxf' using driver `DXF' successful.

    # OGRFeature(entities):0
    #   EntityHandle (String) = 1EF
    #   POINT Z (4 4 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (4 4 0)")

    # OGRFeature(entities):1
    #   EntityHandle (String) = 1F0
    #   LINESTRING Z (0 0 0,1 1 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (0 0 0,1 1 0)")

    # OGRFeature(entities):2
    #   EntityHandle (String) = 1F1
    #   LINESTRING (1 1,2 1,1 2,1 1)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING (1 1,2 1,1 2,1 1)")

    # OGRFeature(entities):3
    #   EntityHandle (String) = 1F2
    #   LINESTRING Z (1 1 0,1 2 0,2 2 0,1 1 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (1 1 0,1 2 0,2 2 0,1 1 0)")

    # OGRFeature(entities):4
    #   EntityHandle (String) = 1F7
    #   LINESTRING Z (6 4 0,5.99512810051965 3.86048705251175 0,5.98053613748314 3.72165379807987 0,5.95629520146761 3.58417661836448 0,5.92252339187664 3.448725288366 0,5.87938524157182 3.31595971334866 0,5.8270909152852 3.1865267138484 0,5.76589518571785 3.06105687442822 0,5.69609619231285 2.94016147153359 0,5.61803398874989 2.82442949541505 0,5.53208888623796 2.71442478062692 0,5.4386796006773 2.61068325908201 0,5.33826121271772 2.51371034904521 0,5.23132295065132 2.42397849278656 0,5.11838580694149 2.34192485488992 0,5.0 2.26794919243112 0,4.87674229357815 2.20241190740167 0,4.74921318683182 2.14563229086643 0,4.61803398874989 2.09788696740969 0,4.48384379119934 2.05940854744801 0,4.34729635533386 2.03038449397558 0,4.20905692653531 2.01095620926345 0,4.069798993405 2.00121834596181 0,3.930201006595 2.00121834596181 0,3.79094307346469 2.01095620926345 0,3.65270364466614 2.03038449397558 0,3.51615620880066 2.05940854744801 0,3.38196601125011 2.09788696740969 0,3.25078681316818 2.14563229086643 0,3.12325770642185 2.20241190740167 0,3.0 2.26794919243112 0,2.88161419305851 2.34192485488992 0,2.76867704934868 2.42397849278656 0,2.66173878728228 2.51371034904521 0,2.5613203993227 2.61068325908201 0,2.46791111376204 2.71442478062692 0,2.38196601125011 2.82442949541505 0,2.30390380768715 2.94016147153359 0,2.23410481428215 3.06105687442822 0,2.1729090847148 3.1865267138484 0,2.12061475842818 3.31595971334866 0,2.07747660812336 3.448725288366 0,2.04370479853239 3.58417661836448 0,2.01946386251686 3.72165379807987 0,2.00487189948035 3.86048705251175 0,2.0 4.0 0,2.00487189948035 4.13951294748825 0,2.01946386251686 4.27834620192013 0,2.04370479853239 4.41582338163552 0,2.07747660812336 4.551274711634 0,2.12061475842818 4.68404028665134 0,2.1729090847148 4.8134732861516 0,2.23410481428215 4.93894312557178 0,2.30390380768715 5.05983852846641 0,2.38196601125011 5.17557050458495 0,2.46791111376204 5.28557521937308 0,2.5613203993227 5.38931674091799 0,2.66173878728228 5.48628965095479 0,2.76867704934868 5.57602150721344 0,2.88161419305851 5.65807514511008 0,3.0 5.73205080756888 0,3.12325770642184 5.79758809259833 0,3.25078681316818 5.85436770913357 0,3.38196601125011 5.90211303259031 0,3.51615620880066 5.94059145255199 0,3.65270364466614 5.96961550602442 0,3.79094307346469 5.98904379073655 0,3.930201006595 5.99878165403819 0,4.069798993405 5.99878165403819 0,4.20905692653531 5.98904379073655 0,4.34729635533386 5.96961550602442 0,4.48384379119933 5.94059145255199 0,4.61803398874989 5.90211303259031 0,4.74921318683182 5.85436770913357 0,4.87674229357815 5.79758809259833 0,5.0 5.73205080756888 0,5.11838580694149 5.65807514511008 0,5.23132295065132 5.57602150721344 0,5.33826121271772 5.48628965095479 0,5.4386796006773 5.389316740918 0,5.53208888623796 5.28557521937308 0,5.61803398874989 5.17557050458495 0,5.69609619231285 5.05983852846641 0,5.76589518571785 4.93894312557178 0,5.8270909152852 4.8134732861516 0,5.87938524157182 4.68404028665134 0,5.92252339187664 4.551274711634 0,5.95629520146761 4.41582338163552 0,5.98053613748314 4.27834620192013 0,5.99512810051965 4.13951294748825 0,6.0 4.0 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (6 4 0,5.99512810051965 3.86048705251175 0,5.98053613748314 3.72165379807987 0,5.95629520146761 3.58417661836448 0,5.92252339187664 3.448725288366 0,5.87938524157182 3.31595971334866 0,5.8270909152852 3.1865267138484 0,5.76589518571785 3.06105687442822 0,5.69609619231285 2.94016147153359 0,5.61803398874989 2.82442949541505 0,5.53208888623796 2.71442478062692 0,5.4386796006773 2.61068325908201 0,5.33826121271772 2.51371034904521 0,5.23132295065132 2.42397849278656 0,5.11838580694149 2.34192485488992 0,5.0 2.26794919243112 0,4.87674229357815 2.20241190740167 0,4.74921318683182 2.14563229086643 0,4.61803398874989 2.09788696740969 0,4.48384379119934 2.05940854744801 0,4.34729635533386 2.03038449397558 0,4.20905692653531 2.01095620926345 0,4.069798993405 2.00121834596181 0,3.930201006595 2.00121834596181 0,3.79094307346469 2.01095620926345 0,3.65270364466614 2.03038449397558 0,3.51615620880066 2.05940854744801 0,3.38196601125011 2.09788696740969 0,3.25078681316818 2.14563229086643 0,3.12325770642185 2.20241190740167 0,3.0 2.26794919243112 0,2.88161419305851 2.34192485488992 0,2.76867704934868 2.42397849278656 0,2.66173878728228 2.51371034904521 0,2.5613203993227 2.61068325908201 0,2.46791111376204 2.71442478062692 0,2.38196601125011 2.82442949541505 0,2.30390380768715 2.94016147153359 0,2.23410481428215 3.06105687442822 0,2.1729090847148 3.1865267138484 0,2.12061475842818 3.31595971334866 0,2.07747660812336 3.448725288366 0,2.04370479853239 3.58417661836448 0,2.01946386251686 3.72165379807987 0,2.00487189948035 3.86048705251175 0,2.0 4.0 0,2.00487189948035 4.13951294748825 0,2.01946386251686 4.27834620192013 0,2.04370479853239 4.41582338163552 0,2.07747660812336 4.551274711634 0,2.12061475842818 4.68404028665134 0,2.1729090847148 4.8134732861516 0,2.23410481428215 4.93894312557178 0,2.30390380768715 5.05983852846641 0,2.38196601125011 5.17557050458495 0,2.46791111376204 5.28557521937308 0,2.5613203993227 5.38931674091799 0,2.66173878728228 5.48628965095479 0,2.76867704934868 5.57602150721344 0,2.88161419305851 5.65807514511008 0,3.0 5.73205080756888 0,3.12325770642184 5.79758809259833 0,3.25078681316818 5.85436770913357 0,3.38196601125011 5.90211303259031 0,3.51615620880066 5.94059145255199 0,3.65270364466614 5.96961550602442 0,3.79094307346469 5.98904379073655 0,3.930201006595 5.99878165403819 0,4.069798993405 5.99878165403819 0,4.20905692653531 5.98904379073655 0,4.34729635533386 5.96961550602442 0,4.48384379119933 5.94059145255199 0,4.61803398874989 5.90211303259031 0,4.74921318683182 5.85436770913357 0,4.87674229357815 5.79758809259833 0,5.0 5.73205080756888 0,5.11838580694149 5.65807514511008 0,5.23132295065132 5.57602150721344 0,5.33826121271772 5.48628965095479 0,5.4386796006773 5.389316740918 0,5.53208888623796 5.28557521937308 0,5.61803398874989 5.17557050458495 0,5.69609619231285 5.05983852846641 0,5.76589518571785 4.93894312557178 0,5.8270909152852 4.8134732861516 0,5.87938524157182 4.68404028665134 0,5.92252339187664 4.551274711634 0,5.95629520146761 4.41582338163552 0,5.98053613748314 4.27834620192013 0,5.99512810051965 4.13951294748825 0,6.0 4.0 0)",
    )

    # OGRFeature(entities):5
    #   EntityHandle (String) = 1F8
    #   LINESTRING Z (2 4 0,2.00487189948035 4.06975647374412 0,2.01946386251686 4.13917310096007 0,2.04370479853239 4.20791169081776 0,2.07747660812336 4.275637355817 0,2.12061475842818 4.34202014332567 0,2.1729090847148 4.4067366430758 0,2.23410481428215 4.46947156278589 0,2.30390380768715 4.52991926423321 0,2.38196601125011 4.58778525229247 0,2.46791111376204 4.64278760968654 0,2.5613203993227 4.694658370459 0,2.66173878728228 4.74314482547739 0,2.76867704934868 4.78801075360672 0,2.88161419305851 4.82903757255504 0,3.0 4.86602540378444 0,3.12325770642185 4.89879404629917 0,3.25078681316818 4.92718385456679 0,3.38196601125011 4.95105651629515 0,3.51615620880067 4.970295726276 0,3.65270364466614 4.98480775301221 0,3.79094307346469 4.99452189536827 0,3.930201006595 4.9993908270191 0,4.069798993405 4.9993908270191 0,4.20905692653531 4.99452189536827 0,4.34729635533386 4.98480775301221 0,4.48384379119934 4.970295726276 0,4.61803398874989 4.95105651629515 0,4.74921318683182 4.92718385456679 0,4.87674229357816 4.89879404629917 0,5.0 4.86602540378444 0,5.11838580694149 4.82903757255504 0,5.23132295065132 4.78801075360672 0,5.33826121271772 4.74314482547739 0,5.4386796006773 4.694658370459 0,5.53208888623796 4.64278760968654 0,5.61803398874989 4.58778525229247 0,5.69609619231285 4.5299192642332 0,5.76589518571785 4.46947156278589 0,5.8270909152852 4.4067366430758 0,5.87938524157182 4.34202014332567 0,5.92252339187664 4.275637355817 0,5.95629520146761 4.20791169081776 0,5.98053613748314 4.13917310096006 0,5.99512810051965 4.06975647374412 0,6.0 4.0 0,5.99512810051965 3.93024352625587 0,5.98053613748314 3.86082689903993 0,5.95629520146761 3.79208830918224 0,5.92252339187664 3.724362644183 0,5.87938524157182 3.65797985667433 0,5.8270909152852 3.5932633569242 0,5.76589518571785 3.53052843721411 0,5.69609619231285 3.4700807357668 0,5.61803398874989 3.41221474770753 0,5.53208888623796 3.35721239031346 0,5.4386796006773 3.305341629541 0,5.33826121271772 3.25685517452261 0,5.23132295065132 3.21198924639328 0,5.11838580694149 3.17096242744496 0,5.0 3.13397459621556 0,4.87674229357815 3.10120595370083 0,4.74921318683182 3.07281614543321 0,4.61803398874989 3.04894348370485 0,4.48384379119934 3.029704273724 0,4.34729635533386 3.01519224698779 0,4.20905692653531 3.00547810463173 0,4.069798993405 3.0006091729809 0,3.930201006595 3.0006091729809 0,3.79094307346469 3.00547810463173 0,3.65270364466614 3.01519224698779 0,3.51615620880066 3.029704273724 0,3.38196601125011 3.04894348370485 0,3.25078681316818 3.07281614543321 0,3.12325770642185 3.10120595370083 0,3.0 3.13397459621556 0,2.88161419305851 3.17096242744496 0,2.76867704934868 3.21198924639328 0,2.66173878728228 3.25685517452261 0,2.5613203993227 3.305341629541 0,2.46791111376204 3.35721239031346 0,2.38196601125011 3.41221474770753 0,2.30390380768715 3.4700807357668 0,2.23410481428215 3.53052843721411 0,2.1729090847148 3.5932633569242 0,2.12061475842818 3.65797985667433 0,2.07747660812336 3.724362644183 0,2.04370479853239 3.79208830918224 0,2.01946386251686 3.86082689903993 0,2.00487189948035 3.93024352625587 0,2 4 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 4 0,2.00487189948035 4.06975647374412 0,2.01946386251686 4.13917310096007 0,2.04370479853239 4.20791169081776 0,2.07747660812336 4.275637355817 0,2.12061475842818 4.34202014332567 0,2.1729090847148 4.4067366430758 0,2.23410481428215 4.46947156278589 0,2.30390380768715 4.52991926423321 0,2.38196601125011 4.58778525229247 0,2.46791111376204 4.64278760968654 0,2.5613203993227 4.694658370459 0,2.66173878728228 4.74314482547739 0,2.76867704934868 4.78801075360672 0,2.88161419305851 4.82903757255504 0,3.0 4.86602540378444 0,3.12325770642185 4.89879404629917 0,3.25078681316818 4.92718385456679 0,3.38196601125011 4.95105651629515 0,3.51615620880067 4.970295726276 0,3.65270364466614 4.98480775301221 0,3.79094307346469 4.99452189536827 0,3.930201006595 4.9993908270191 0,4.069798993405 4.9993908270191 0,4.20905692653531 4.99452189536827 0,4.34729635533386 4.98480775301221 0,4.48384379119934 4.970295726276 0,4.61803398874989 4.95105651629515 0,4.74921318683182 4.92718385456679 0,4.87674229357816 4.89879404629917 0,5.0 4.86602540378444 0,5.11838580694149 4.82903757255504 0,5.23132295065132 4.78801075360672 0,5.33826121271772 4.74314482547739 0,5.4386796006773 4.694658370459 0,5.53208888623796 4.64278760968654 0,5.61803398874989 4.58778525229247 0,5.69609619231285 4.5299192642332 0,5.76589518571785 4.46947156278589 0,5.8270909152852 4.4067366430758 0,5.87938524157182 4.34202014332567 0,5.92252339187664 4.275637355817 0,5.95629520146761 4.20791169081776 0,5.98053613748314 4.13917310096006 0,5.99512810051965 4.06975647374412 0,6.0 4.0 0,5.99512810051965 3.93024352625587 0,5.98053613748314 3.86082689903993 0,5.95629520146761 3.79208830918224 0,5.92252339187664 3.724362644183 0,5.87938524157182 3.65797985667433 0,5.8270909152852 3.5932633569242 0,5.76589518571785 3.53052843721411 0,5.69609619231285 3.4700807357668 0,5.61803398874989 3.41221474770753 0,5.53208888623796 3.35721239031346 0,5.4386796006773 3.305341629541 0,5.33826121271772 3.25685517452261 0,5.23132295065132 3.21198924639328 0,5.11838580694149 3.17096242744496 0,5.0 3.13397459621556 0,4.87674229357815 3.10120595370083 0,4.74921318683182 3.07281614543321 0,4.61803398874989 3.04894348370485 0,4.48384379119934 3.029704273724 0,4.34729635533386 3.01519224698779 0,4.20905692653531 3.00547810463173 0,4.069798993405 3.0006091729809 0,3.930201006595 3.0006091729809 0,3.79094307346469 3.00547810463173 0,3.65270364466614 3.01519224698779 0,3.51615620880066 3.029704273724 0,3.38196601125011 3.04894348370485 0,3.25078681316818 3.07281614543321 0,3.12325770642185 3.10120595370083 0,3.0 3.13397459621556 0,2.88161419305851 3.17096242744496 0,2.76867704934868 3.21198924639328 0,2.66173878728228 3.25685517452261 0,2.5613203993227 3.305341629541 0,2.46791111376204 3.35721239031346 0,2.38196601125011 3.41221474770753 0,2.30390380768715 3.4700807357668 0,2.23410481428215 3.53052843721411 0,2.1729090847148 3.5932633569242 0,2.12061475842818 3.65797985667433 0,2.07747660812336 3.724362644183 0,2.04370479853239 3.79208830918224 0,2.01946386251686 3.86082689903993 0,2.00487189948035 3.93024352625587 0,2 4 0)",
    )

    # OGRFeature(entities):6
    #   EntityHandle (String) = 1F9
    #   LINESTRING Z (2.0 2.0 0,1.96657794502105 2.03582232791524 0,1.93571660708646 2.07387296203834 0,1.90756413746468 2.11396923855471 0,1.88225568337755 2.15591867344963 0,1.85991273921989 2.19951988653655 0,1.84064256332004 2.24456356819194 0,1.8245376630414 2.29083348415575 0,1.81167535069652 2.33810751357387 0,1.80211737240583 2.38615871529951 0,1.79590961168258 2.43475641733454 0,1.79308186916688 2.48366732418105 0,1.79364771956639 2.53265663678705 0,1.79760444649032 2.58148917971011 0,1.80493305548955 2.62993053008785 0,1.81559836524041 2.67774814299566 0,1.8295491764342 2.72471246778926 0,1.84671851756181 2.7705980500731 0,1.86702396641357 2.81518461400453 0,1.89036804575079 2.85825811973811 0,1.91663869124976 2.89961179093366 0,1.94570978947168 2.93904710739563 0,1.97744178327594 2.97637475807832 0,2.01168234177068 3.01141554988232 0,2.04826709158413 3.04400126787917 0,2.08702040594658 3.07397548283483 0,2.12775624779472 3.10119430215541 0,2.17027906285109 3.12552706065018 0,2.2143847183914 3.14685694779575 0,2.25986148319297 3.16508156849045 0,2.30649104396024 3.18011343460661 0,2.35404955334774 3.1918803849814 0,2.40230870454951 3.20032593182975 0,2.45103682729644 3.2054095319166 0,2.5 3.20710678118655 0,2.54896317270356 3.2054095319166 0,2.59769129545049 3.20032593182975 0,2.64595044665226 3.1918803849814 0,2.69350895603976 3.18011343460661 0,2.74013851680703 3.16508156849045 0,2.7856152816086 3.14685694779575 0,2.8297209371489 3.12552706065018 0,2.87224375220528 3.10119430215541 0,2.91297959405342 3.07397548283483 0,2.95173290841587 3.04400126787917 0,2.98831765822932 3.01141554988232 0,3.02255821672406 2.97637475807832 0,3.05429021052832 2.93904710739563 0,3.08336130875024 2.89961179093367 0,3.10963195424921 2.85825811973811 0,3.13297603358643 2.81518461400453 0,3.15328148243819 2.7705980500731 0,3.1704508235658 2.72471246778926 0,3.18440163475959 2.67774814299567 0,3.19506694451045 2.62993053008786 0,3.20239555350968 2.58148917971011 0,3.20635228043361 2.53265663678705 0,3.20691813083312 2.48366732418105 0,3.20409038831742 2.43475641733454 0,3.19788262759417 2.38615871529951 0,3.18832464930348 2.33810751357387 0,3.1754623369586 2.29083348415575 0,3.15935743667996 2.24456356819194 0,3.14008726078011 2.19951988653655 0,3.11774431662245 2.15591867344963 0,3.09243586253532 2.11396923855472 0,3.06428339291354 2.07387296203834 0,3.03342205497895 2.03582232791524 0,3 2 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2.0 2.0 0,1.96657794502105 2.03582232791524 0,1.93571660708646 2.07387296203834 0,1.90756413746468 2.11396923855471 0,1.88225568337755 2.15591867344963 0,1.85991273921989 2.19951988653655 0,1.84064256332004 2.24456356819194 0,1.8245376630414 2.29083348415575 0,1.81167535069652 2.33810751357387 0,1.80211737240583 2.38615871529951 0,1.79590961168258 2.43475641733454 0,1.79308186916688 2.48366732418105 0,1.79364771956639 2.53265663678705 0,1.79760444649032 2.58148917971011 0,1.80493305548955 2.62993053008785 0,1.81559836524041 2.67774814299566 0,1.8295491764342 2.72471246778926 0,1.84671851756181 2.7705980500731 0,1.86702396641357 2.81518461400453 0,1.89036804575079 2.85825811973811 0,1.91663869124976 2.89961179093366 0,1.94570978947168 2.93904710739563 0,1.97744178327594 2.97637475807832 0,2.01168234177068 3.01141554988232 0,2.04826709158413 3.04400126787917 0,2.08702040594658 3.07397548283483 0,2.12775624779472 3.10119430215541 0,2.17027906285109 3.12552706065018 0,2.2143847183914 3.14685694779575 0,2.25986148319297 3.16508156849045 0,2.30649104396024 3.18011343460661 0,2.35404955334774 3.1918803849814 0,2.40230870454951 3.20032593182975 0,2.45103682729644 3.2054095319166 0,2.5 3.20710678118655 0,2.54896317270356 3.2054095319166 0,2.59769129545049 3.20032593182975 0,2.64595044665226 3.1918803849814 0,2.69350895603976 3.18011343460661 0,2.74013851680703 3.16508156849045 0,2.7856152816086 3.14685694779575 0,2.8297209371489 3.12552706065018 0,2.87224375220528 3.10119430215541 0,2.91297959405342 3.07397548283483 0,2.95173290841587 3.04400126787917 0,2.98831765822932 3.01141554988232 0,3.02255821672406 2.97637475807832 0,3.05429021052832 2.93904710739563 0,3.08336130875024 2.89961179093367 0,3.10963195424921 2.85825811973811 0,3.13297603358643 2.81518461400453 0,3.15328148243819 2.7705980500731 0,3.1704508235658 2.72471246778926 0,3.18440163475959 2.67774814299567 0,3.19506694451045 2.62993053008786 0,3.20239555350968 2.58148917971011 0,3.20635228043361 2.53265663678705 0,3.20691813083312 2.48366732418105 0,3.20409038831742 2.43475641733454 0,3.19788262759417 2.38615871529951 0,3.18832464930348 2.33810751357387 0,3.1754623369586 2.29083348415575 0,3.15935743667996 2.24456356819194 0,3.14008726078011 2.19951988653655 0,3.11774431662245 2.15591867344963 0,3.09243586253532 2.11396923855472 0,3.06428339291354 2.07387296203834 0,3.03342205497895 2.03582232791524 0,3 2 0)",
    )

    # OGRFeature(entities):7
    #   EntityHandle (String) = 1FA
    #   POLYGON Z ((1 2 0,1 3 0,2 3 0,2 2 0,1 2 0))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POLYGON Z ((1 2 0,1 3 0,2 3 0,2 2 0,1 2 0))")

    # OGRFeature(entities):8
    #   EntityHandle (String) = 1FB
    #   POLYGON ((3 4,4 4,4 3,3 3,3 4))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POLYGON ((3 4,4 4,4 3,3 3,3 4))")

    # OGRFeature(entities):9
    #   EntityHandle (String) = 1FD
    #   POLYGON Z ((8 8 0,9 8 0,9 9 0,8 9 0,8 8 0))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POLYGON Z ((8 8 0,9 8 0,9 9 0,8 9 0,8 8 0))")

    # OGRFeature(entities):10
    #   EntityHandle (String) = 200
    #   LINESTRING Z (2 2 0,2.15384615384615 2.15384615384615 0,2.30769230769231 2.30769230769231 0,2.46153846153846 2.46153846153846 0,2.61538461538461 2.61538461538461 0,2.76923076923077 2.76923076923077 0,2.92307692307692 2.92307692307692 0,3.07692307692308 3.07692307692308 0,3.23076923076923 3.23076923076923 0,3.38461538461538 3.38461538461538 0,3.53846153846154 3.53846153846154 0,3.69230769230769 3.69230769230769 0,3.84615384615385 3.84615384615385 0,4 4 0,4.15384615384615 4.15384615384615 0,4.30769230769231 4.30769230769231 0,4.46153846153846 4.46153846153846 0,4.61538461538462 4.61538461538462 0,4.76923076923077 4.76923076923077 0,4.92307692307692 4.92307692307692 0,5.07692307692308 5.07692307692308 0,5.23076923076923 5.23076923076923 0,5.38461538461538 5.38461538461538 0,5.53846153846154 5.53846153846154 0,5.69230769230769 5.69230769230769 0,5.84615384615385 5.84615384615385 0,6.0 6.0 0,6.15384615384615 6.15384615384615 0,6.30769230769231 6.30769230769231 0,6.46153846153846 6.46153846153846 0,6.61538461538462 6.61538461538462 0,6.76923076923077 6.76923076923077 0,6.92307692307692 6.92307692307692 0,7.07692307692308 7.07692307692308 0,7.23076923076923 7.23076923076923 0,7.38461538461539 7.38461538461539 0,7.53846153846154 7.53846153846154 0,7.69230769230769 7.69230769230769 0,7.84615384615385 7.84615384615385 0,8 8 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 2 0,2.15384615384615 2.15384615384615 0,2.30769230769231 2.30769230769231 0,2.46153846153846 2.46153846153846 0,2.61538461538461 2.61538461538461 0,2.76923076923077 2.76923076923077 0,2.92307692307692 2.92307692307692 0,3.07692307692308 3.07692307692308 0,3.23076923076923 3.23076923076923 0,3.38461538461538 3.38461538461538 0,3.53846153846154 3.53846153846154 0,3.69230769230769 3.69230769230769 0,3.84615384615385 3.84615384615385 0,4 4 0,4.15384615384615 4.15384615384615 0,4.30769230769231 4.30769230769231 0,4.46153846153846 4.46153846153846 0,4.61538461538462 4.61538461538462 0,4.76923076923077 4.76923076923077 0,4.92307692307692 4.92307692307692 0,5.07692307692308 5.07692307692308 0,5.23076923076923 5.23076923076923 0,5.38461538461538 5.38461538461538 0,5.53846153846154 5.53846153846154 0,5.69230769230769 5.69230769230769 0,5.84615384615385 5.84615384615385 0,6.0 6.0 0,6.15384615384615 6.15384615384615 0,6.30769230769231 6.30769230769231 0,6.46153846153846 6.46153846153846 0,6.61538461538462 6.61538461538462 0,6.76923076923077 6.76923076923077 0,6.92307692307692 6.92307692307692 0,7.07692307692308 7.07692307692308 0,7.23076923076923 7.23076923076923 0,7.38461538461539 7.38461538461539 0,7.53846153846154 7.53846153846154 0,7.69230769230769 7.69230769230769 0,7.84615384615385 7.84615384615385 0,8 8 0)",
    )

    # OGRFeature(entities):11
    #   EntityHandle (String) = 201
    #   LINESTRING Z (8 1 0,7.62837370825536 0.987348067229724 0,7.25775889681215 0.975707614760869 0,6.88916704597178 0.966090122894857 0,6.52360963603567 0.959507071933107 0,6.16209814730525 0.956969942177043 0,5.80564406008193 0.959490213928084 0,5.45525885466714 0.968079367487651 0,5.11195401136229 0.983748883157167 0,4.77674101046882 1.00751024123805 0,4.45063133228814 1.04037492203173 0,4.13463645712167 1.08335440583961 0,3.82976786527082 1.13746017296313 0,3.53703703703704 1.2037037037037 0,3.25745545272173 1.28309647836275 0,2.99203459262631 1.37664997724169 0,2.74178593705221 1.48537568064195 0,2.50772096630085 1.61028506886495 0,2.29085116067365 1.75238962221211 0,2.09218800047203 1.91270082098484 0,1.91270082098485 2.09218800047202 0,1.75238962221211 2.29085116067364 0,1.61028506886495 2.50772096630085 0,1.48537568064195 2.74178593705221 0,1.37664997724169 2.99203459262631 0,1.28309647836275 3.25745545272172 0,1.2037037037037 3.53703703703703 0,1.13746017296313 3.82976786527082 0,1.08335440583961 4.13463645712166 0,1.04037492203173 4.45063133228814 0,1.00751024123805 4.77674101046882 0,0.983748883157167 5.11195401136229 0,0.968079367487652 5.45525885466714 0,0.959490213928084 5.80564406008193 0,0.956969942177043 6.16209814730525 0,0.959507071933108 6.52360963603567 0,0.966090122894857 6.88916704597178 0,0.975707614760869 7.25775889681216 0,0.987348067229724 7.62837370825537 0,1 8 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (8 1 0,7.62837370825536 0.987348067229724 0,7.25775889681215 0.975707614760869 0,6.88916704597178 0.966090122894857 0,6.52360963603567 0.959507071933107 0,6.16209814730525 0.956969942177043 0,5.80564406008193 0.959490213928084 0,5.45525885466714 0.968079367487651 0,5.11195401136229 0.983748883157167 0,4.77674101046882 1.00751024123805 0,4.45063133228814 1.04037492203173 0,4.13463645712167 1.08335440583961 0,3.82976786527082 1.13746017296313 0,3.53703703703704 1.2037037037037 0,3.25745545272173 1.28309647836275 0,2.99203459262631 1.37664997724169 0,2.74178593705221 1.48537568064195 0,2.50772096630085 1.61028506886495 0,2.29085116067365 1.75238962221211 0,2.09218800047203 1.91270082098484 0,1.91270082098485 2.09218800047202 0,1.75238962221211 2.29085116067364 0,1.61028506886495 2.50772096630085 0,1.48537568064195 2.74178593705221 0,1.37664997724169 2.99203459262631 0,1.28309647836275 3.25745545272172 0,1.2037037037037 3.53703703703703 0,1.13746017296313 3.82976786527082 0,1.08335440583961 4.13463645712166 0,1.04037492203173 4.45063133228814 0,1.00751024123805 4.77674101046882 0,0.983748883157167 5.11195401136229 0,0.968079367487652 5.45525885466714 0,0.959490213928084 5.80564406008193 0,0.956969942177043 6.16209814730525 0,0.959507071933108 6.52360963603567 0,0.966090122894857 6.88916704597178 0,0.975707614760869 7.25775889681216 0,0.987348067229724 7.62837370825537 0,1 8 0)",
    )

    # OGRFeature(entities):12
    #   EntityHandle (String) = 202
    #   POINT Z (7 7 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (7 7 0)")

    # OGRFeature(entities):13
    #   EntityHandle (String) = 203
    #   POINT Z (-4 4 -5e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (-4 4 -5e-16)")

    # OGRFeature(entities):14
    #   EntityHandle (String) = 204
    #   LINESTRING Z (0 0 0,-1 1 -1e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (0 0 0,-1 1 -1e-16)")

    # OGRFeature(entities):15
    #   EntityHandle (String) = 205
    #   LINESTRING Z (-1 1 -1E-16,-2 1 -2E-16,-1 2 -1E-16,-1 1 -1E-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (-1 1 -1E-16,-2 1 -2E-16,-1 2 -1E-16,-1 1 -1E-16)"
    )

    # OGRFeature(entities):16
    #   EntityHandle (String) = 206
    #   LINESTRING Z (-1 1 -1e-16,-1 2 -1e-16,-2 2 -2e-16,-1 1 -1e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (-1 1 -1e-16,-1 2 -1e-16,-2 2 -2e-16,-1 1 -1e-16)"
    )

    # OGRFeature(entities):17
    #   EntityHandle (String) = 20B
    #   LINESTRING Z (-6 4 -6e-16,-5.99512810051965 3.86048705251175 -5.99512810051965e-16,-5.98053613748314 3.72165379807987 -5.98053613748314e-16,-5.95629520146761 3.58417661836448 -5.95629520146761e-16,-5.92252339187664 3.448725288366 -5.92252339187664e-16,-5.87938524157182 3.31595971334866 -5.87938524157182e-16,-5.8270909152852 3.1865267138484 -5.8270909152852e-16,-5.76589518571785 3.06105687442822 -5.76589518571785e-16,-5.69609619231285 2.94016147153359 -5.69609619231285e-16,-5.61803398874989 2.82442949541505 -5.61803398874989e-16,-5.53208888623796 2.71442478062692 -5.53208888623796e-16,-5.4386796006773 2.61068325908201 -5.4386796006773e-16,-5.33826121271772 2.51371034904521 -5.33826121271772e-16,-5.23132295065132 2.42397849278656 -5.23132295065132e-16,-5.11838580694149 2.34192485488992 -5.11838580694149e-16,-5.0 2.26794919243112 -5e-16,-4.87674229357815 2.20241190740167 -4.87674229357815e-16,-4.74921318683182 2.14563229086643 -4.74921318683182e-16,-4.61803398874989 2.09788696740969 -4.61803398874989e-16,-4.48384379119934 2.05940854744801 -4.48384379119934e-16,-4.34729635533386 2.03038449397558 -4.34729635533386e-16,-4.20905692653531 2.01095620926345 -4.20905692653531e-16,-4.069798993405 2.00121834596181 -4.069798993405e-16,-3.930201006595 2.00121834596181 -3.930201006595e-16,-3.79094307346469 2.01095620926345 -3.79094307346469e-16,-3.65270364466614 2.03038449397558 -3.65270364466614e-16,-3.51615620880066 2.05940854744801 -3.51615620880066e-16,-3.38196601125011 2.09788696740969 -3.3819660112501e-16,-3.25078681316818 2.14563229086643 -3.25078681316818e-16,-3.12325770642185 2.20241190740167 -3.12325770642185e-16,-3.0 2.26794919243112 -3e-16,-2.88161419305851 2.34192485488992 -2.88161419305851e-16,-2.76867704934868 2.42397849278656 -2.76867704934868e-16,-2.66173878728228 2.51371034904521 -2.66173878728228e-16,-2.5613203993227 2.61068325908201 -2.5613203993227e-16,-2.46791111376204 2.71442478062692 -2.46791111376204e-16,-2.38196601125011 2.82442949541505 -2.3819660112501e-16,-2.30390380768715 2.94016147153359 -2.30390380768715e-16,-2.23410481428215 3.06105687442822 -2.23410481428215e-16,-2.1729090847148 3.1865267138484 -2.1729090847148e-16,-2.12061475842818 3.31595971334866 -2.12061475842818e-16,-2.07747660812336 3.448725288366 -2.07747660812336e-16,-2.04370479853239 3.58417661836448 -2.04370479853239e-16,-2.01946386251686 3.72165379807987 -2.01946386251686e-16,-2.00487189948035 3.86048705251175 -2.00487189948035e-16,-2.0 4.0 -2e-16,-2.00487189948035 4.13951294748825 -2.00487189948035e-16,-2.01946386251686 4.27834620192013 -2.01946386251686e-16,-2.04370479853239 4.41582338163552 -2.04370479853239e-16,-2.07747660812336 4.551274711634 -2.07747660812336e-16,
    # -2.12061475842818 4.68404028665134 -2.12061475842818e-16,-2.1729090847148 4.8134732861516 -2.1729090847148e-16,-2.23410481428215 4.93894312557178 -2.23410481428215e-16,-2.30390380768715 5.05983852846641 -2.30390380768715e-16,-2.38196601125011 5.17557050458495 -2.3819660112501e-16,-2.46791111376204 5.28557521937308 -2.46791111376204e-16,-2.5613203993227 5.38931674091799 -2.5613203993227e-16,-2.66173878728228 5.48628965095479 -2.66173878728228e-16,-2.76867704934868 5.57602150721344 -2.76867704934868e-16,-2.88161419305851 5.65807514511008 -2.88161419305851e-16,-3.0 5.73205080756888 -3e-16,-3.12325770642184 5.79758809259833 -3.12325770642184e-16,-3.25078681316818 5.85436770913357 -3.25078681316817e-16,-3.38196601125011 5.90211303259031 -3.3819660112501e-16,-3.51615620880066 5.94059145255199 -3.51615620880066e-16,-3.65270364466614 5.96961550602442 -3.65270364466614e-16,-3.79094307346469 5.98904379073655 -3.79094307346469e-16,-3.930201006595 5.99878165403819 -3.930201006595e-16,-4.069798993405 5.99878165403819 -4.069798993405e-16,-4.20905692653531 5.98904379073655 -4.20905692653531e-16,-4.34729635533386 5.96961550602442 -4.34729635533386e-16,-4.48384379119933 5.94059145255199 -4.48384379119933e-16,-4.61803398874989 5.90211303259031 -4.61803398874989e-16,-4.74921318683182 5.85436770913357 -4.74921318683182e-16,-4.87674229357815 5.79758809259833 -4.87674229357815e-16,-5.0 5.73205080756888 -5e-16,-5.11838580694149 5.65807514511008 -5.11838580694149e-16,-5.23132295065132 5.57602150721344 -5.23132295065132e-16,-5.33826121271772 5.48628965095479 -5.33826121271772e-16,-5.4386796006773 5.389316740918 -5.4386796006773e-16,-5.53208888623796 5.28557521937308 -5.53208888623796e-16,-5.61803398874989 5.17557050458495 -5.61803398874989e-16,-5.69609619231285 5.05983852846641 -5.69609619231285e-16,-5.76589518571785 4.93894312557178 -5.76589518571785e-16,-5.8270909152852 4.8134732861516 -5.8270909152852e-16,-5.87938524157182 4.68404028665134 -5.87938524157182e-16,-5.92252339187664 4.551274711634 -5.92252339187664e-16,-5.95629520146761 4.41582338163552 -5.95629520146761e-16,-5.98053613748314 4.27834620192013 -5.98053613748314e-16,-5.99512810051965 4.13951294748825 -5.99512810051965e-16,-6.0 4.0 -6e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-6 4 -6e-16,-5.99512810051965 3.86048705251175 -5.99512810051965e-16,-5.98053613748314 3.72165379807987 -5.98053613748314e-16,-5.95629520146761 3.58417661836448 -5.95629520146761e-16,-5.92252339187664 3.448725288366 -5.92252339187664e-16,-5.87938524157182 3.31595971334866 -5.87938524157182e-16,-5.8270909152852 3.1865267138484 -5.8270909152852e-16,-5.76589518571785 3.06105687442822 -5.76589518571785e-16,-5.69609619231285 2.94016147153359 -5.69609619231285e-16,-5.61803398874989 2.82442949541505 -5.61803398874989e-16,-5.53208888623796 2.71442478062692 -5.53208888623796e-16,-5.4386796006773 2.61068325908201 -5.4386796006773e-16,-5.33826121271772 2.51371034904521 -5.33826121271772e-16,-5.23132295065132 2.42397849278656 -5.23132295065132e-16,-5.11838580694149 2.34192485488992 -5.11838580694149e-16,-5.0 2.26794919243112 -5e-16,-4.87674229357815 2.20241190740167 -4.87674229357815e-16,-4.74921318683182 2.14563229086643 -4.74921318683182e-16,-4.61803398874989 2.09788696740969 -4.61803398874989e-16,-4.48384379119934 2.05940854744801 -4.48384379119934e-16,-4.34729635533386 2.03038449397558 -4.34729635533386e-16,-4.20905692653531 2.01095620926345 -4.20905692653531e-16,-4.069798993405 2.00121834596181 -4.069798993405e-16,-3.930201006595 2.00121834596181 -3.930201006595e-16,-3.79094307346469 2.01095620926345 -3.79094307346469e-16,-3.65270364466614 2.03038449397558 -3.65270364466614e-16,-3.51615620880066 2.05940854744801 -3.51615620880066e-16,-3.38196601125011 2.09788696740969 -3.3819660112501e-16,-3.25078681316818 2.14563229086643 -3.25078681316818e-16,-3.12325770642185 2.20241190740167 -3.12325770642185e-16,-3.0 2.26794919243112 -3e-16,-2.88161419305851 2.34192485488992 -2.88161419305851e-16,-2.76867704934868 2.42397849278656 -2.76867704934868e-16,-2.66173878728228 2.51371034904521 -2.66173878728228e-16,-2.5613203993227 2.61068325908201 -2.5613203993227e-16,-2.46791111376204 2.71442478062692 -2.46791111376204e-16,-2.38196601125011 2.82442949541505 -2.3819660112501e-16,-2.30390380768715 2.94016147153359 -2.30390380768715e-16,-2.23410481428215 3.06105687442822 -2.23410481428215e-16,-2.1729090847148 3.1865267138484 -2.1729090847148e-16,-2.12061475842818 3.31595971334866 -2.12061475842818e-16,-2.07747660812336 3.448725288366 -2.07747660812336e-16,-2.04370479853239 3.58417661836448 -2.04370479853239e-16,-2.01946386251686 3.72165379807987 -2.01946386251686e-16,-2.00487189948035 3.86048705251175 -2.00487189948035e-16,-2.0 4.0 -2e-16,-2.00487189948035 4.13951294748825 -2.00487189948035e-16,-2.01946386251686 4.27834620192013 -2.01946386251686e-16,-2.04370479853239 4.41582338163552 -2.04370479853239e-16,"
        + "-2.07747660812336 4.551274711634 -2.07747660812336e-16,-2.12061475842818 4.68404028665134 -2.12061475842818e-16,-2.1729090847148 4.8134732861516 -2.1729090847148e-16,-2.23410481428215 4.93894312557178 -2.23410481428215e-16,-2.30390380768715 5.05983852846641 -2.30390380768715e-16,-2.38196601125011 5.17557050458495 -2.3819660112501e-16,-2.46791111376204 5.28557521937308 -2.46791111376204e-16,-2.5613203993227 5.38931674091799 -2.5613203993227e-16,-2.66173878728228 5.48628965095479 -2.66173878728228e-16,-2.76867704934868 5.57602150721344 -2.76867704934868e-16,-2.88161419305851 5.65807514511008 -2.88161419305851e-16,-3.0 5.73205080756888 -3e-16,-3.12325770642184 5.79758809259833 -3.12325770642184e-16,-3.25078681316818 5.85436770913357 -3.25078681316817e-16,-3.38196601125011 5.90211303259031 -3.3819660112501e-16,-3.51615620880066 5.94059145255199 -3.51615620880066e-16,-3.65270364466614 5.96961550602442 -3.65270364466614e-16,-3.79094307346469 5.98904379073655 -3.79094307346469e-16,-3.930201006595 5.99878165403819 -3.930201006595e-16,-4.069798993405 5.99878165403819 -4.069798993405e-16,-4.20905692653531 5.98904379073655 -4.20905692653531e-16,-4.34729635533386 5.96961550602442 -4.34729635533386e-16,-4.48384379119933 5.94059145255199 -4.48384379119933e-16,-4.61803398874989 5.90211303259031 -4.61803398874989e-16,-4.74921318683182 5.85436770913357 -4.74921318683182e-16,-4.87674229357815 5.79758809259833 -4.87674229357815e-16,-5.0 5.73205080756888 -5e-16,-5.11838580694149 5.65807514511008 -5.11838580694149e-16,-5.23132295065132 5.57602150721344 -5.23132295065132e-16,-5.33826121271772 5.48628965095479 -5.33826121271772e-16,-5.4386796006773 5.389316740918 -5.4386796006773e-16,-5.53208888623796 5.28557521937308 -5.53208888623796e-16,-5.61803398874989 5.17557050458495 -5.61803398874989e-16,-5.69609619231285 5.05983852846641 -5.69609619231285e-16,-5.76589518571785 4.93894312557178 -5.76589518571785e-16,-5.8270909152852 4.8134732861516 -5.8270909152852e-16,-5.87938524157182 4.68404028665134 -5.87938524157182e-16,-5.92252339187664 4.551274711634 -5.92252339187664e-16,-5.95629520146761 4.41582338163552 -5.95629520146761e-16,-5.98053613748314 4.27834620192013 -5.98053613748314e-16,-5.99512810051965 4.13951294748825 -5.99512810051965e-16,-6.0 4.0 -6e-16)",
    )

    # OGRFeature(entities):18
    #   EntityHandle (String) = 20C
    #   LINESTRING Z (-2 4 -3e-16,-2.00487189948035 4.06975647374412 -3.00487189948035e-16,-2.01946386251686 4.13917310096007 -3.01946386251686e-16,-2.04370479853239 4.20791169081776 -3.04370479853239e-16,-2.07747660812336 4.275637355817 -3.07747660812336e-16,-2.12061475842818 4.34202014332567 -3.12061475842818e-16,-2.1729090847148 4.4067366430758 -3.1729090847148e-16,-2.23410481428215 4.46947156278589 -3.23410481428215e-16,-2.30390380768715 4.52991926423321 -3.30390380768715e-16,-2.38196601125011 4.58778525229247 -3.38196601125011e-16,-2.46791111376204 4.64278760968654 -3.46791111376204e-16,-2.5613203993227 4.694658370459 -3.5613203993227e-16,-2.66173878728228 4.74314482547739 -3.66173878728228e-16,-2.76867704934868 4.78801075360672 -3.76867704934868e-16,-2.88161419305851 4.82903757255504 -3.88161419305851e-16,-3.0 4.86602540378444 -4e-16,-3.12325770642185 4.89879404629917 -4.12325770642185e-16,-3.25078681316818 4.92718385456679 -4.25078681316818e-16,-3.38196601125011 4.95105651629515 -4.38196601125011e-16,-3.51615620880067 4.970295726276 -4.51615620880067e-16,-3.65270364466614 4.98480775301221 -4.65270364466614e-16,-3.79094307346469 4.99452189536827 -4.79094307346469e-16,-3.930201006595 4.9993908270191 -4.930201006595e-16,-4.069798993405 4.9993908270191 -5.069798993405e-16,-4.20905692653531 4.99452189536827 -5.20905692653531e-16,-4.34729635533386 4.98480775301221 -5.34729635533386e-16,-4.48384379119934 4.970295726276 -5.48384379119934e-16,-4.61803398874989 4.95105651629515 -5.6180339887499e-16,-4.74921318683182 4.92718385456679 -5.74921318683183e-16,-4.87674229357816 4.89879404629917 -5.87674229357816e-16,-5.0 4.86602540378444 -6e-16,-5.11838580694149 4.82903757255504 -6.1183858069415e-16,-5.23132295065132 4.78801075360672 -6.23132295065132e-16,-5.33826121271772 4.74314482547739 -6.33826121271772e-16,-5.4386796006773 4.694658370459 -6.4386796006773e-16,-5.53208888623796 4.64278760968654 -6.53208888623796e-16,-5.61803398874989 4.58778525229247 -6.61803398874989e-16,-5.69609619231285 4.5299192642332 -6.69609619231285e-16,-5.76589518571785 4.46947156278589 -6.76589518571785e-16,-5.8270909152852 4.4067366430758 -6.8270909152852e-16,-5.87938524157182 4.34202014332567 -6.87938524157182e-16,-5.92252339187664 4.275637355817 -6.92252339187664e-16,-5.95629520146761 4.20791169081776 -6.95629520146761e-16,-5.98053613748314 4.13917310096006 -6.98053613748314e-16,-5.99512810051965 4.06975647374412 -6.99512810051965e-16,-6.0 4.0 -7e-16,-5.99512810051965 3.93024352625587 -6.99512810051965e-16,-5.98053613748314 3.86082689903993 -6.98053613748314e-16,-5.95629520146761 3.79208830918224 -6.95629520146761e-16,
    # -5.92252339187664 3.724362644183 -6.92252339187664e-16,-5.87938524157182 3.65797985667433 -6.87938524157182e-16,-5.8270909152852 3.5932633569242 -6.8270909152852e-16,-5.76589518571785 3.53052843721411 -6.76589518571785e-16,-5.69609619231285 3.4700807357668 -6.69609619231285e-16,-5.61803398874989 3.41221474770753 -6.61803398874989e-16,-5.53208888623796 3.35721239031346 -6.53208888623796e-16,-5.4386796006773 3.305341629541 -6.4386796006773e-16,-5.33826121271772 3.25685517452261 -6.33826121271772e-16,-5.23132295065132 3.21198924639328 -6.23132295065132e-16,-5.11838580694149 3.17096242744496 -6.11838580694149e-16,-5.0 3.13397459621556 -6e-16,-4.87674229357815 3.10120595370083 -5.87674229357816e-16,-4.74921318683182 3.07281614543321 -5.74921318683182e-16,-4.61803398874989 3.04894348370485 -5.6180339887499e-16,-4.48384379119934 3.029704273724 -5.48384379119934e-16,-4.34729635533386 3.01519224698779 -5.34729635533386e-16,-4.20905692653531 3.00547810463173 -5.20905692653531e-16,-4.069798993405 3.0006091729809 -5.069798993405e-16,-3.930201006595 3.0006091729809 -4.930201006595e-16,-3.79094307346469 3.00547810463173 -4.79094307346469e-16,-3.65270364466614 3.01519224698779 -4.65270364466614e-16,-3.51615620880066 3.029704273724 -4.51615620880066e-16,-3.38196601125011 3.04894348370485 -4.38196601125011e-16,-3.25078681316818 3.07281614543321 -4.25078681316818e-16,-3.12325770642185 3.10120595370083 -4.12325770642185e-16,-3.0 3.13397459621556 -4e-16,-2.88161419305851 3.17096242744496 -3.88161419305851e-16,-2.76867704934868 3.21198924639328 -3.76867704934868e-16,-2.66173878728228 3.25685517452261 -3.66173878728228e-16,-2.5613203993227 3.305341629541 -3.5613203993227e-16,-2.46791111376204 3.35721239031346 -3.46791111376204e-16,-2.38196601125011 3.41221474770753 -3.38196601125011e-16,-2.30390380768715 3.4700807357668 -3.30390380768715e-16,-2.23410481428215 3.53052843721411 -3.23410481428215e-16,-2.1729090847148 3.5932633569242 -3.1729090847148e-16,-2.12061475842818 3.65797985667433 -3.12061475842818e-16,-2.07747660812336 3.724362644183 -3.07747660812336e-16,-2.04370479853239 3.79208830918224 -3.04370479853239e-16,-2.01946386251686 3.86082689903993 -3.01946386251686e-16,-2.00487189948035 3.93024352625587 -3.00487189948035e-16,-2 4 -3e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-2 4 -3e-16,-2.00487189948035 4.06975647374412 -3.00487189948035e-16,-2.01946386251686 4.13917310096007 -3.01946386251686e-16,-2.04370479853239 4.20791169081776 -3.04370479853239e-16,-2.07747660812336 4.275637355817 -3.07747660812336e-16,-2.12061475842818 4.34202014332567 -3.12061475842818e-16,-2.1729090847148 4.4067366430758 -3.1729090847148e-16,-2.23410481428215 4.46947156278589 -3.23410481428215e-16,-2.30390380768715 4.52991926423321 -3.30390380768715e-16,-2.38196601125011 4.58778525229247 -3.38196601125011e-16,-2.46791111376204 4.64278760968654 -3.46791111376204e-16,-2.5613203993227 4.694658370459 -3.5613203993227e-16,-2.66173878728228 4.74314482547739 -3.66173878728228e-16,-2.76867704934868 4.78801075360672 -3.76867704934868e-16,-2.88161419305851 4.82903757255504 -3.88161419305851e-16,-3.0 4.86602540378444 -4e-16,-3.12325770642185 4.89879404629917 -4.12325770642185e-16,-3.25078681316818 4.92718385456679 -4.25078681316818e-16,-3.38196601125011 4.95105651629515 -4.38196601125011e-16,-3.51615620880067 4.970295726276 -4.51615620880067e-16,-3.65270364466614 4.98480775301221 -4.65270364466614e-16,-3.79094307346469 4.99452189536827 -4.79094307346469e-16,-3.930201006595 4.9993908270191 -4.930201006595e-16,-4.069798993405 4.9993908270191 -5.069798993405e-16,-4.20905692653531 4.99452189536827 -5.20905692653531e-16,-4.34729635533386 4.98480775301221 -5.34729635533386e-16,-4.48384379119934 4.970295726276 -5.48384379119934e-16,-4.61803398874989 4.95105651629515 -5.6180339887499e-16,-4.74921318683182 4.92718385456679 -5.74921318683183e-16,-4.87674229357816 4.89879404629917 -5.87674229357816e-16,-5.0 4.86602540378444 -6e-16,-5.11838580694149 4.82903757255504 -6.1183858069415e-16,-5.23132295065132 4.78801075360672 -6.23132295065132e-16,-5.33826121271772 4.74314482547739 -6.33826121271772e-16,-5.4386796006773 4.694658370459 -6.4386796006773e-16,-5.53208888623796 4.64278760968654 -6.53208888623796e-16,-5.61803398874989 4.58778525229247 -6.61803398874989e-16,-5.69609619231285 4.5299192642332 -6.69609619231285e-16,-5.76589518571785 4.46947156278589 -6.76589518571785e-16,-5.8270909152852 4.4067366430758 -6.8270909152852e-16,-5.87938524157182 4.34202014332567 -6.87938524157182e-16,-5.92252339187664 4.275637355817 -6.92252339187664e-16,-5.95629520146761 4.20791169081776 -6.95629520146761e-16,-5.98053613748314 4.13917310096006 -6.98053613748314e-16,-5.99512810051965 4.06975647374412 -6.99512810051965e-16,-6.0 4.0 -7e-16,-5.99512810051965 3.93024352625587 -6.99512810051965e-16,-5.98053613748314 3.86082689903993 -6.98053613748314e-16,-5.95629520146761 3.79208830918224 -6.95629520146761e-16,"
        + "-5.92252339187664 3.724362644183 -6.92252339187664e-16,-5.87938524157182 3.65797985667433 -6.87938524157182e-16,-5.8270909152852 3.5932633569242 -6.8270909152852e-16,-5.76589518571785 3.53052843721411 -6.76589518571785e-16,-5.69609619231285 3.4700807357668 -6.69609619231285e-16,-5.61803398874989 3.41221474770753 -6.61803398874989e-16,-5.53208888623796 3.35721239031346 -6.53208888623796e-16,-5.4386796006773 3.305341629541 -6.4386796006773e-16,-5.33826121271772 3.25685517452261 -6.33826121271772e-16,-5.23132295065132 3.21198924639328 -6.23132295065132e-16,-5.11838580694149 3.17096242744496 -6.11838580694149e-16,-5.0 3.13397459621556 -6e-16,-4.87674229357815 3.10120595370083 -5.87674229357816e-16,-4.74921318683182 3.07281614543321 -5.74921318683182e-16,-4.61803398874989 3.04894348370485 -5.6180339887499e-16,-4.48384379119934 3.029704273724 -5.48384379119934e-16,-4.34729635533386 3.01519224698779 -5.34729635533386e-16,-4.20905692653531 3.00547810463173 -5.20905692653531e-16,-4.069798993405 3.0006091729809 -5.069798993405e-16,-3.930201006595 3.0006091729809 -4.930201006595e-16,-3.79094307346469 3.00547810463173 -4.79094307346469e-16,-3.65270364466614 3.01519224698779 -4.65270364466614e-16,-3.51615620880066 3.029704273724 -4.51615620880066e-16,-3.38196601125011 3.04894348370485 -4.38196601125011e-16,-3.25078681316818 3.07281614543321 -4.25078681316818e-16,-3.12325770642185 3.10120595370083 -4.12325770642185e-16,-3.0 3.13397459621556 -4e-16,-2.88161419305851 3.17096242744496 -3.88161419305851e-16,-2.76867704934868 3.21198924639328 -3.76867704934868e-16,-2.66173878728228 3.25685517452261 -3.66173878728228e-16,-2.5613203993227 3.305341629541 -3.5613203993227e-16,-2.46791111376204 3.35721239031346 -3.46791111376204e-16,-2.38196601125011 3.41221474770753 -3.38196601125011e-16,-2.30390380768715 3.4700807357668 -3.30390380768715e-16,-2.23410481428215 3.53052843721411 -3.23410481428215e-16,-2.1729090847148 3.5932633569242 -3.1729090847148e-16,-2.12061475842818 3.65797985667433 -3.12061475842818e-16,-2.07747660812336 3.724362644183 -3.07747660812336e-16,-2.04370479853239 3.79208830918224 -3.04370479853239e-16,-2.01946386251686 3.86082689903993 -3.01946386251686e-16,-2.00487189948035 3.93024352625587 -3.00487189948035e-16,-2 4 -3e-16)",
    )

    # OGRFeature(entities):19
    #   EntityHandle (String) = 20D
    #   LINESTRING Z (-2.0 2.0 -2e-16,-1.96657794502105 2.03582232791524 -1.96657794502105e-16,-1.93571660708646 2.07387296203834 -1.93571660708646e-16,-1.90756413746468 2.11396923855471 -1.90756413746468e-16,-1.88225568337755 2.15591867344963 -1.88225568337755e-16,-1.85991273921989 2.19951988653655 -1.85991273921989e-16,-1.84064256332004 2.24456356819194 -1.84064256332004e-16,-1.8245376630414 2.29083348415575 -1.8245376630414e-16,-1.81167535069652 2.33810751357387 -1.81167535069652e-16,-1.80211737240583 2.38615871529951 -1.80211737240583e-16,-1.79590961168258 2.43475641733454 -1.79590961168258e-16,-1.79308186916688 2.48366732418105 -1.79308186916688e-16,-1.79364771956639 2.53265663678705 -1.79364771956639e-16,-1.79760444649032 2.58148917971011 -1.79760444649032e-16,-1.80493305548955 2.62993053008785 -1.80493305548955e-16,-1.81559836524041 2.67774814299566 -1.81559836524041e-16,-1.8295491764342 2.72471246778926 -1.8295491764342e-16,-1.84671851756181 2.7705980500731 -1.84671851756181e-16,-1.86702396641357 2.81518461400453 -1.86702396641357e-16,-1.89036804575079 2.85825811973811 -1.89036804575079e-16,-1.91663869124976 2.89961179093366 -1.91663869124976e-16,-1.94570978947168 2.93904710739563 -1.94570978947168e-16,-1.97744178327594 2.97637475807832 -1.97744178327594e-16,-2.01168234177068 3.01141554988232 -2.01168234177068e-16,-2.04826709158413 3.04400126787917 -2.04826709158413e-16,-2.08702040594658 3.07397548283483 -2.08702040594658e-16,-2.12775624779472 3.10119430215541 -2.12775624779472e-16,-2.17027906285109 3.12552706065018 -2.17027906285109e-16,-2.2143847183914 3.14685694779575 -2.2143847183914e-16,-2.25986148319297 3.16508156849045 -2.25986148319297e-16,-2.30649104396024 3.18011343460661 -2.30649104396024e-16,-2.35404955334774 3.1918803849814 -2.35404955334774e-16,-2.40230870454951 3.20032593182975 -2.40230870454951e-16,-2.45103682729644 3.2054095319166 -2.45103682729644e-16,-2.5 3.20710678118655 -2.5e-16,-2.54896317270356 3.2054095319166 -2.54896317270356e-16,-2.59769129545049 3.20032593182975 -2.59769129545049e-16,-2.64595044665226 3.1918803849814 -2.64595044665226e-16,-2.69350895603976 3.18011343460661 -2.69350895603976e-16,-2.74013851680703 3.16508156849045 -2.74013851680703e-16,-2.7856152816086 3.14685694779575 -2.7856152816086e-16,-2.8297209371489 3.12552706065018 -2.8297209371489e-16,-2.87224375220528 3.10119430215541 -2.87224375220528e-16,-2.91297959405342 3.07397548283483 -2.91297959405342e-16,-2.95173290841587 3.04400126787917 -2.95173290841587e-16,-2.98831765822932 3.01141554988232 -2.98831765822932e-16,-3.02255821672406 2.97637475807832 -3.02255821672406e-16,-3.05429021052832 2.93904710739563 -3.05429021052832e-16,-3.08336130875024 2.89961179093367 -3.08336130875024e-16,-3.10963195424921 2.85825811973811 -3.10963195424921e-16,-3.13297603358643 2.81518461400453 -3.13297603358643e-16,-3.15328148243819 2.7705980500731 -3.15328148243819e-16,-3.1704508235658 2.72471246778926 -3.1704508235658e-16,-3.18440163475959 2.67774814299567 -3.18440163475959e-16,-3.19506694451045 2.62993053008786 -3.19506694451045e-16,-3.20239555350968 2.58148917971011 -3.20239555350968e-16,-3.20635228043361 2.53265663678705 -3.20635228043361e-16,-3.20691813083312 2.48366732418105 -3.20691813083312e-16,-3.20409038831742 2.43475641733454 -3.20409038831742e-16,-3.19788262759417 2.38615871529951 -3.19788262759417e-16,-3.18832464930348 2.33810751357387 -3.18832464930349e-16,-3.1754623369586 2.29083348415575 -3.1754623369586e-16,-3.15935743667996 2.24456356819194 -3.15935743667996e-16,-3.14008726078011 2.19951988653655 -3.14008726078011e-16,-3.11774431662245 2.15591867344963 -3.11774431662245e-16,-3.09243586253532 2.11396923855472 -3.09243586253532e-16,-3.06428339291354 2.07387296203834 -3.06428339291354e-16,-3.03342205497895 2.03582232791524 -3.03342205497895e-16,-3 2 -3e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-2.0 2.0 -2e-16,-1.96657794502105 2.03582232791524 -1.96657794502105e-16,-1.93571660708646 2.07387296203834 -1.93571660708646e-16,-1.90756413746468 2.11396923855471 -1.90756413746468e-16,-1.88225568337755 2.15591867344963 -1.88225568337755e-16,-1.85991273921989 2.19951988653655 -1.85991273921989e-16,-1.84064256332004 2.24456356819194 -1.84064256332004e-16,-1.8245376630414 2.29083348415575 -1.8245376630414e-16,-1.81167535069652 2.33810751357387 -1.81167535069652e-16,-1.80211737240583 2.38615871529951 -1.80211737240583e-16,-1.79590961168258 2.43475641733454 -1.79590961168258e-16,-1.79308186916688 2.48366732418105 -1.79308186916688e-16,-1.79364771956639 2.53265663678705 -1.79364771956639e-16,-1.79760444649032 2.58148917971011 -1.79760444649032e-16,-1.80493305548955 2.62993053008785 -1.80493305548955e-16,-1.81559836524041 2.67774814299566 -1.81559836524041e-16,-1.8295491764342 2.72471246778926 -1.8295491764342e-16,-1.84671851756181 2.7705980500731 -1.84671851756181e-16,-1.86702396641357 2.81518461400453 -1.86702396641357e-16,-1.89036804575079 2.85825811973811 -1.89036804575079e-16,-1.91663869124976 2.89961179093366 -1.91663869124976e-16,-1.94570978947168 2.93904710739563 -1.94570978947168e-16,-1.97744178327594 2.97637475807832 -1.97744178327594e-16,-2.01168234177068 3.01141554988232 -2.01168234177068e-16,-2.04826709158413 3.04400126787917 -2.04826709158413e-16,-2.08702040594658 3.07397548283483 -2.08702040594658e-16,-2.12775624779472 3.10119430215541 -2.12775624779472e-16,-2.17027906285109 3.12552706065018 -2.17027906285109e-16,-2.2143847183914 3.14685694779575 -2.2143847183914e-16,-2.25986148319297 3.16508156849045 -2.25986148319297e-16,-2.30649104396024 3.18011343460661 -2.30649104396024e-16,-2.35404955334774 3.1918803849814 -2.35404955334774e-16,-2.40230870454951 3.20032593182975 -2.40230870454951e-16,-2.45103682729644 3.2054095319166 -2.45103682729644e-16,"
        + "-2.5 3.20710678118655 -2.5e-16,-2.54896317270356 3.2054095319166 -2.54896317270356e-16,-2.59769129545049 3.20032593182975 -2.59769129545049e-16,-2.64595044665226 3.1918803849814 -2.64595044665226e-16,-2.69350895603976 3.18011343460661 -2.69350895603976e-16,-2.74013851680703 3.16508156849045 -2.74013851680703e-16,-2.7856152816086 3.14685694779575 -2.7856152816086e-16,-2.8297209371489 3.12552706065018 -2.8297209371489e-16,-2.87224375220528 3.10119430215541 -2.87224375220528e-16,-2.91297959405342 3.07397548283483 -2.91297959405342e-16,-2.95173290841587 3.04400126787917 -2.95173290841587e-16,-2.98831765822932 3.01141554988232 -2.98831765822932e-16,-3.02255821672406 2.97637475807832 -3.02255821672406e-16,-3.05429021052832 2.93904710739563 -3.05429021052832e-16,-3.08336130875024 2.89961179093367 -3.08336130875024e-16,-3.10963195424921 2.85825811973811 -3.10963195424921e-16,-3.13297603358643 2.81518461400453 -3.13297603358643e-16,-3.15328148243819 2.7705980500731 -3.15328148243819e-16,-3.1704508235658 2.72471246778926 -3.1704508235658e-16,-3.18440163475959 2.67774814299567 -3.18440163475959e-16,-3.19506694451045 2.62993053008786 -3.19506694451045e-16,-3.20239555350968 2.58148917971011 -3.20239555350968e-16,-3.20635228043361 2.53265663678705 -3.20635228043361e-16,-3.20691813083312 2.48366732418105 -3.20691813083312e-16,-3.20409038831742 2.43475641733454 -3.20409038831742e-16,-3.19788262759417 2.38615871529951 -3.19788262759417e-16,-3.18832464930348 2.33810751357387 -3.18832464930349e-16,-3.1754623369586 2.29083348415575 -3.1754623369586e-16,-3.15935743667996 2.24456356819194 -3.15935743667996e-16,-3.14008726078011 2.19951988653655 -3.14008726078011e-16,-3.11774431662245 2.15591867344963 -3.11774431662245e-16,-3.09243586253532 2.11396923855472 -3.09243586253532e-16,-3.06428339291354 2.07387296203834 -3.06428339291354e-16,-3.03342205497895 2.03582232791524 -3.03342205497895e-16,-3 2 -3e-16)",
    )

    # OGRFeature(entities):20
    #   EntityHandle (String) = 20E
    #   POLYGON Z ((-1 2 -1e-16,-1 3 -1e-16,-2 3 -2e-16,-2 2 -2e-16,-1 2 -1e-16))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((-1 2 -1e-16,-1 3 -1e-16,-2 3 -2e-16,-2 2 -2e-16,-1 2 -1e-16))",
    )

    # OGRFeature(entities):21
    #   EntityHandle (String) = 20F
    #   POLYGON Z ((-3 4 -3E-16,-4 4 -4E-16,-4 3 -4E-16,-3 3 -3E-16,-3 4 -3E-16))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((-3 4 -3E-16,-4 4 -4E-16,-4 3 -4E-16,-3 3 -3E-16,-3 4 -3E-16))",
    )

    # OGRFeature(entities):22
    #   EntityHandle (String) = 211
    #   POLYGON Z ((-8 8 -8E-16,-9 8 -9E-16,-9 9 -9E-16,-8 9 -8E-16,-8 8 -8E-16))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((-8 8 -8E-16,-9 8 -9E-16,-9 9 -9E-16,-8 9 -8E-16,-8 8 -8E-16))",
    )

    # OGRFeature(entities):23
    #   EntityHandle (String) = 212
    #   LINESTRING Z (-2 2 -0.0,-2.15384615384615 2.15384615384615 -0.0,-2.30769230769231 2.30769230769231 -0.0,-2.46153846153846 2.46153846153846 -0.0,-2.61538461538461 2.61538461538461 -0.0,-2.76923076923077 2.76923076923077 -0.0,-2.92307692307692 2.92307692307692 -0.0,-3.07692307692308 3.07692307692308 -0.0,-3.23076923076923 3.23076923076923 -0.0,-3.38461538461538 3.38461538461538 -0.0,-3.53846153846154 3.53846153846154 -0.0,-3.69230769230769 3.69230769230769 -0.0,-3.84615384615385 3.84615384615385 -0.0,-4 4 -0.0,-4.15384615384615 4.15384615384615 -0.0,-4.30769230769231 4.30769230769231 -0.0,-4.46153846153846 4.46153846153846 -0.0,-4.61538461538462 4.61538461538462 -0.0,-4.76923076923077 4.76923076923077 -0.0,-4.92307692307692 4.92307692307692 -0.0,-5.07692307692308 5.07692307692308 -0.0,-5.23076923076923 5.23076923076923 -0.0,-5.38461538461538 5.38461538461538 -0.0,-5.53846153846154 5.53846153846154 -0.0,-5.69230769230769 5.69230769230769 -0.0,-5.84615384615385 5.84615384615385 -0.0,-6 6.0 -0.0,-6.15384615384615 6.15384615384615 -0.0,-6.30769230769231 6.30769230769231 -0.0,-6.46153846153846 6.46153846153846 -0.0,-6.61538461538462 6.61538461538462 -0.0,-6.76923076923077 6.76923076923077 -0.0,-6.92307692307692 6.92307692307692 -0.0,-7.07692307692308 7.07692307692308 -0.0,-7.23076923076923 7.23076923076923 -0.0,-7.38461538461539 7.38461538461539 -0.0,-7.53846153846154 7.53846153846154 -0.0,-7.69230769230769 7.69230769230769 -0.0,-7.84615384615385 7.84615384615385 -0.0,-8 8 -0.0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-2 2 -0.0,-2.15384615384615 2.15384615384615 -0.0,-2.30769230769231 2.30769230769231 -0.0,-2.46153846153846 2.46153846153846 -0.0,-2.61538461538461 2.61538461538461 -0.0,-2.76923076923077 2.76923076923077 -0.0,-2.92307692307692 2.92307692307692 -0.0,-3.07692307692308 3.07692307692308 -0.0,-3.23076923076923 3.23076923076923 -0.0,-3.38461538461538 3.38461538461538 -0.0,-3.53846153846154 3.53846153846154 -0.0,-3.69230769230769 3.69230769230769 -0.0,-3.84615384615385 3.84615384615385 -0.0,-4 4 -0.0,-4.15384615384615 4.15384615384615 -0.0,-4.30769230769231 4.30769230769231 -0.0,-4.46153846153846 4.46153846153846 -0.0,-4.61538461538462 4.61538461538462 -0.0,-4.76923076923077 4.76923076923077 -0.0,-4.92307692307692 4.92307692307692 -0.0,-5.07692307692308 5.07692307692308 -0.0,-5.23076923076923 5.23076923076923 -0.0,-5.38461538461538 5.38461538461538 -0.0,-5.53846153846154 5.53846153846154 -0.0,-5.69230769230769 5.69230769230769 -0.0,-5.84615384615385 5.84615384615385 -0.0,-6 6.0 -0.0,-6.15384615384615 6.15384615384615 -0.0,-6.30769230769231 6.30769230769231 -0.0,-6.46153846153846 6.46153846153846 -0.0,-6.61538461538462 6.61538461538462 -0.0,-6.76923076923077 6.76923076923077 -0.0,-6.92307692307692 6.92307692307692 -0.0,-7.07692307692308 7.07692307692308 -0.0,-7.23076923076923 7.23076923076923 -0.0,-7.38461538461539 7.38461538461539 -0.0,-7.53846153846154 7.53846153846154 -0.0,-7.69230769230769 7.69230769230769 -0.0,-7.84615384615385 7.84615384615385 -0.0,-8 8 -0.0)",
    )

    # OGRFeature(entities):24
    #   EntityHandle (String) = 213
    #   LINESTRING Z (-8 1 -0.0,-7.62837370825536 0.987348067229724 -0.0,-7.25775889681215 0.975707614760869 -0.0,-6.88916704597178 0.966090122894857 -0.0,-6.52360963603567 0.959507071933107 -0.0,-6.16209814730525 0.956969942177043 -0.0,-5.80564406008193 0.959490213928084 -0.0,-5.45525885466714 0.968079367487651 -0.0,-5.11195401136229 0.983748883157167 -0.0,-4.77674101046882 1.00751024123805 -0.0,-4.45063133228814 1.04037492203173 -0.0,-4.13463645712167 1.08335440583961 -0.0,-3.82976786527082 1.13746017296313 -0.0,-3.53703703703704 1.2037037037037 -0.0,-3.25745545272173 1.28309647836275 -0.0,-2.99203459262631 1.37664997724169 -0.0,-2.74178593705221 1.48537568064195 -0.0,-2.50772096630085 1.61028506886495 -0.0,-2.29085116067365 1.75238962221211 -0.0,-2.09218800047203 1.91270082098484 -0.0,-1.91270082098485 2.09218800047202 -0.0,-1.75238962221211 2.29085116067364 -0.0,-1.61028506886495 2.50772096630085 -0.0,-1.48537568064195 2.74178593705221 -0.0,-1.37664997724169 2.99203459262631 -0.0,-1.28309647836275 3.25745545272172 -0.0,-1.2037037037037 3.53703703703703 -0.0,-1.13746017296313 3.82976786527082 -0.0,-1.08335440583961 4.13463645712166 -0.0,-1.04037492203173 4.45063133228814 -0.0,-1.00751024123805 4.77674101046882 -0.0,-0.983748883157167 5.11195401136229 -0.0,-0.968079367487652 5.45525885466714 -0.0,-0.959490213928084 5.80564406008193 -0.0,-0.956969942177043 6.16209814730525 -0.0,-0.959507071933108 6.52360963603567 -0.0,-0.966090122894857 6.88916704597178 -0.0,-0.975707614760869 7.25775889681216 -0.0,-0.987348067229724 7.62837370825537 -0.0,-1 8 -0.0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-8 1 -0.0,-7.62837370825536 0.987348067229724 -0.0,-7.25775889681215 0.975707614760869 -0.0,-6.88916704597178 0.966090122894857 -0.0,-6.52360963603567 0.959507071933107 -0.0,-6.16209814730525 0.956969942177043 -0.0,-5.80564406008193 0.959490213928084 -0.0,-5.45525885466714 0.968079367487651 -0.0,-5.11195401136229 0.983748883157167 -0.0,-4.77674101046882 1.00751024123805 -0.0,-4.45063133228814 1.04037492203173 -0.0,-4.13463645712167 1.08335440583961 -0.0,-3.82976786527082 1.13746017296313 -0.0,-3.53703703703704 1.2037037037037 -0.0,-3.25745545272173 1.28309647836275 -0.0,-2.99203459262631 1.37664997724169 -0.0,-2.74178593705221 1.48537568064195 -0.0,-2.50772096630085 1.61028506886495 -0.0,-2.29085116067365 1.75238962221211 -0.0,-2.09218800047203 1.91270082098484 -0.0,-1.91270082098485 2.09218800047202 -0.0,-1.75238962221211 2.29085116067364 -0.0,-1.61028506886495 2.50772096630085 -0.0,-1.48537568064195 2.74178593705221 -0.0,-1.37664997724169 2.99203459262631 -0.0,-1.28309647836275 3.25745545272172 -0.0,-1.2037037037037 3.53703703703703 -0.0,-1.13746017296313 3.82976786527082 -0.0,-1.08335440583961 4.13463645712166 -0.0,-1.04037492203173 4.45063133228814 -0.0,-1.00751024123805 4.77674101046882 -0.0,-0.983748883157167 5.11195401136229 -0.0,-0.968079367487652 5.45525885466714 -0.0,-0.959490213928084 5.80564406008193 -0.0,-0.956969942177043 6.16209814730525 -0.0,-0.959507071933108 6.52360963603567 -0.0,-0.966090122894857 6.88916704597178 -0.0,-0.975707614760869 7.25775889681216 -0.0,-0.987348067229724 7.62837370825537 -0.0,-1 8 -0.0)",
    )

    # OGRFeature(entities):25
    #   EntityHandle (String) = 214
    #   POINT Z (-7 7 -7e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (-7 7 -7e-16)")

    # OGRFeature(entities):26
    #   EntityHandle (String) = 215
    #   POINT Z (-4 -4 -1e-15)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (-4 -4 -1e-15)")

    # OGRFeature(entities):27
    #   EntityHandle (String) = 216
    #   LINESTRING Z (0 0 -2e-16,-1 -1 -5e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (0 0 -2e-16,-1 -1 -5e-16)")

    # OGRFeature(entities):28
    #   EntityHandle (String) = 217
    #   LINESTRING (-1 -1,-2 -1,-1 -2,-1 -1)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING (-1 -1,-2 -1,-1 -2,-1 -1)")

    # OGRFeature(entities):29
    #   EntityHandle (String) = 218
    #   LINESTRING Z (-1 -1 -2e-16,-1 -2 -4e-16,-2 -2 -5e-16,-1 -1 -2e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (-1 -1 -2e-16,-1 -2 -4e-16,-2 -2 -5e-16,-1 -1 -2e-16)"
    )

    # OGRFeature(entities):30
    #   EntityHandle (String) = 21D
    #   LINESTRING Z (-2 -4 0,-2.00487189948035 -4.13951294748825 0,-2.01946386251686 -4.27834620192013 0,-2.04370479853239 -4.41582338163552 0,-2.07747660812336 -4.551274711634 0,-2.12061475842818 -4.68404028665134 0,-2.1729090847148 -4.8134732861516 0,-2.23410481428215 -4.93894312557178 0,-2.30390380768715 -5.05983852846641 0,-2.38196601125011 -5.17557050458495 0,-2.46791111376204 -5.28557521937308 0,-2.5613203993227 -5.38931674091799 0,-2.66173878728228 -5.48628965095479 0,-2.76867704934868 -5.57602150721344 0,-2.88161419305851 -5.65807514511008 0,-3.0 -5.73205080756888 0,-3.12325770642185 -5.79758809259833 0,-3.25078681316818 -5.85436770913357 0,-3.38196601125011 -5.90211303259031 0,-3.51615620880066 -5.94059145255199 0,-3.65270364466614 -5.96961550602442 0,-3.79094307346469 -5.98904379073655 0,-3.930201006595 -5.99878165403819 0,-4.069798993405 -5.99878165403819 0,-4.20905692653531 -5.98904379073655 0,-4.34729635533386 -5.96961550602442 0,-4.48384379119934 -5.94059145255199 0,-4.61803398874989 -5.90211303259031 0,-4.74921318683182 -5.85436770913357 0,-4.87674229357815 -5.79758809259833 0,-5.0 -5.73205080756888 0,-5.11838580694149 -5.65807514511008 0,-5.23132295065132 -5.57602150721344 0,-5.33826121271772 -5.48628965095479 0,-5.4386796006773 -5.38931674091799 0,-5.53208888623796 -5.28557521937308 0,-5.61803398874989 -5.17557050458495 0,-5.69609619231285 -5.05983852846641 0,-5.76589518571785 -4.93894312557178 0,-5.8270909152852 -4.8134732861516 0,-5.87938524157182 -4.68404028665134 0,-5.92252339187664 -4.551274711634 0,-5.95629520146761 -4.41582338163552 0,-5.98053613748314 -4.27834620192013 0,-5.99512810051965 -4.13951294748825 0,-6 -4 0,-5.99512810051965 -3.86048705251175 0,-5.98053613748314 -3.72165379807987 0,-5.95629520146761 -3.58417661836448 0,-5.92252339187664 -3.448725288366 0,-5.87938524157182 -3.31595971334866 0,-5.8270909152852 -3.1865267138484 0,-5.76589518571785 -3.06105687442822 0,-5.69609619231285 -2.94016147153359 0,-5.61803398874989 -2.82442949541505 0,-5.53208888623796 -2.71442478062692 0,-5.4386796006773 -2.61068325908201 0,-5.33826121271772 -2.51371034904521 0,-5.23132295065132 -2.42397849278656 0,-5.11838580694149 -2.34192485488992 0,-5.0 -2.26794919243112 0,-4.87674229357816 -2.20241190740167 0,-4.74921318683182 -2.14563229086643 0,-4.61803398874989 -2.09788696740969 0,-4.48384379119934 -2.05940854744801 0,-4.34729635533386 -2.03038449397558 0,-4.20905692653531 -2.01095620926345 0,-4.069798993405 -2.00121834596181 0,-3.930201006595 -2.00121834596181 0,-3.79094307346469 -2.01095620926345 0,-3.65270364466614 -2.03038449397558 0,-3.51615620880067 -2.05940854744801 0,-3.38196601125011 -2.09788696740969 0,-3.25078681316818 -2.14563229086643 0,-3.12325770642185 -2.20241190740167 0,-3.0 -2.26794919243112 0,-2.88161419305851 -2.34192485488992 0,-2.76867704934868 -2.42397849278656 0,-2.66173878728228 -2.51371034904521 0,-2.5613203993227 -2.610683259082 0,-2.46791111376204 -2.71442478062692 0,-2.38196601125011 -2.82442949541505 0,-2.30390380768715 -2.94016147153359 0,-2.23410481428215 -3.06105687442822 0,-2.1729090847148 -3.1865267138484 0,-2.12061475842818 -3.31595971334866 0,-2.07747660812336 -3.448725288366 0,-2.04370479853239 -3.58417661836448 0,-2.01946386251686 -3.72165379807987 0,-2.00487189948035 -3.86048705251175 0,-2.0 -4.0 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-2 -4 0,-2.00487189948035 -4.13951294748825 0,-2.01946386251686 -4.27834620192013 0,-2.04370479853239 -4.41582338163552 0,-2.07747660812336 -4.551274711634 0,-2.12061475842818 -4.68404028665134 0,-2.1729090847148 -4.8134732861516 0,-2.23410481428215 -4.93894312557178 0,-2.30390380768715 -5.05983852846641 0,-2.38196601125011 -5.17557050458495 0,-2.46791111376204 -5.28557521937308 0,-2.5613203993227 -5.38931674091799 0,-2.66173878728228 -5.48628965095479 0,-2.76867704934868 -5.57602150721344 0,-2.88161419305851 -5.65807514511008 0,-3.0 -5.73205080756888 0,-3.12325770642185 -5.79758809259833 0,-3.25078681316818 -5.85436770913357 0,-3.38196601125011 -5.90211303259031 0,-3.51615620880066 -5.94059145255199 0,-3.65270364466614 -5.96961550602442 0,-3.79094307346469 -5.98904379073655 0,-3.930201006595 -5.99878165403819 0,-4.069798993405 -5.99878165403819 0,-4.20905692653531 -5.98904379073655 0,-4.34729635533386 -5.96961550602442 0,-4.48384379119934 -5.94059145255199 0,-4.61803398874989 -5.90211303259031 0,-4.74921318683182 -5.85436770913357 0,-4.87674229357815 -5.79758809259833 0,-5.0 -5.73205080756888 0,-5.11838580694149 -5.65807514511008 0,-5.23132295065132 -5.57602150721344 0,-5.33826121271772 -5.48628965095479 0,-5.4386796006773 -5.38931674091799 0,-5.53208888623796 -5.28557521937308 0,-5.61803398874989 -5.17557050458495 0,-5.69609619231285 -5.05983852846641 0,-5.76589518571785 -4.93894312557178 0,-5.8270909152852 -4.8134732861516 0,-5.87938524157182 -4.68404028665134 0,-5.92252339187664 -4.551274711634 0,-5.95629520146761 -4.41582338163552 0,-5.98053613748314 -4.27834620192013 0,-5.99512810051965 -4.13951294748825 0,-6 -4 0,-5.99512810051965 -3.86048705251175 0,-5.98053613748314 -3.72165379807987 0,-5.95629520146761 -3.58417661836448 0,-5.92252339187664 -3.448725288366 0,-5.87938524157182 -3.31595971334866 0,-5.8270909152852 -3.1865267138484 0,-5.76589518571785 -3.06105687442822 0,-5.69609619231285 -2.94016147153359 0,-5.61803398874989 -2.82442949541505 0,-5.53208888623796 -2.71442478062692 0,-5.4386796006773 -2.61068325908201 0,-5.33826121271772 -2.51371034904521 0,-5.23132295065132 -2.42397849278656 0,-5.11838580694149 -2.34192485488992 0,-5.0 -2.26794919243112 0,-4.87674229357816 -2.20241190740167 0,-4.74921318683182 -2.14563229086643 0,-4.61803398874989 -2.09788696740969 0,-4.48384379119934 -2.05940854744801 0,-4.34729635533386 -2.03038449397558 0,-4.20905692653531 -2.01095620926345 0,-4.069798993405 -2.00121834596181 0,-3.930201006595 -2.00121834596181 0,-3.79094307346469 -2.01095620926345 0,-3.65270364466614 -2.03038449397558 0,-3.51615620880067 -2.05940854744801 0,-3.38196601125011 -2.09788696740969 0,-3.25078681316818 -2.14563229086643 0,-3.12325770642185 -2.20241190740167 0,-3.0 -2.26794919243112 0,-2.88161419305851 -2.34192485488992 0,-2.76867704934868 -2.42397849278656 0,-2.66173878728228 -2.51371034904521 0,-2.5613203993227 -2.610683259082 0,-2.46791111376204 -2.71442478062692 0,-2.38196601125011 -2.82442949541505 0,-2.30390380768715 -2.94016147153359 0,-2.23410481428215 -3.06105687442822 0,-2.1729090847148 -3.1865267138484 0,-2.12061475842818 -3.31595971334866 0,-2.07747660812336 -3.448725288366 0,-2.04370479853239 -3.58417661836448 0,-2.01946386251686 -3.72165379807987 0,-2.00487189948035 -3.86048705251175 0,-2.0 -4.0 0)",
    )

    # OGRFeature(entities):31
    #   EntityHandle (String) = 21E
    #   LINESTRING Z (-2 -4 -8e-16,-2.00487189948035 -4.06975647374412 -8.07462837322448e-16,-2.01946386251686 -4.13917310096007 -8.15863696347693e-16,-2.04370479853239 -4.20791169081776 -8.25161648935015e-16,-2.07747660812336 -4.275637355817 -8.35311396394036e-16,-2.12061475842818 -4.34202014332567 -8.46263490175385e-16,-2.1729090847148 -4.4067366430758 -8.5796457277906e-16,-2.23410481428215 -4.46947156278589 -8.70357637706804e-16,-2.30390380768715 -4.52991926423321 -8.83382307192036e-16,-2.38196601125011 -4.58778525229247 -8.96975126354258e-16,-2.46791111376204 -4.64278760968654 -9.11069872344859e-16,-2.5613203993227 -4.694658370459 -9.2559787697817e-16,-2.66173878728228 -4.74314482547739 -9.40488361275968e-16,-2.76867704934868 -4.78801075360672 -9.5566878029554e-16,-2.88161419305851 -4.82903757255504 -9.71065176561355e-16,-3.0 -4.86602540378444 -9.86602540378444e-16,-3.12325770642185 -4.89879404629917 -1.0022051752721e-15,-3.25078681316818 -4.92718385456679 -1.0177970667735e-15,-3.38196601125011 -4.95105651629515 -1.03330225275453e-15,-3.51615620880067 -4.970295726276 -1.04864519350767e-15,-3.65270364466614 -4.98480775301221 -1.06375113976783e-15,-3.79094307346469 -4.99452189536827 -1.0785464968833e-15,-3.930201006595 -4.9993908270191 -1.09295918336141e-15,-4.069798993405 -4.9993908270191 -1.10691898204241e-15,-4.20905692653531 -4.99452189536827 -1.12035788219036e-15,-4.34729635533386 -4.98480775301221 -1.13321041083461e-15,-4.48384379119934 -4.970295726276 -1.14541395174753e-15,-4.61803398874989 -4.95105651629515 -1.1569090505045e-15,-4.74921318683182 -4.92718385456679 -1.16763970413986e-15,-4.87674229357816 -4.89879404629917 -1.17755363398773e-15,-5.0 -4.86602540378444 -1.18660254037844e-15,-5.11838580694149 -4.82903757255504 -1.19474233794965e-15,-5.23132295065132 -4.78801075360672 -1.2019333704258e-15,-5.33826121271772 -4.74314482547739 -1.20814060381951e-15,-5.4386796006773 -4.694658370459 -1.21333379711363e-15,-5.53208888623796 -4.64278760968654 -1.21748764959245e-15,-5.61803398874989 -4.58778525229247 -1.22058192410424e-15,-5.69609619231285 -4.5299192642332 -1.22260154565461e-15,-5.76589518571785 -4.46947156278589 -1.22353667485037e-15,-5.8270909152852 -4.4067366430758 -1.2233827558361e-15,-5.87938524157182 -4.34202014332567 -1.22214053848975e-15,-5.92252339187664 -4.275637355817 -1.21981607476936e-15,-5.95629520146761 -4.20791169081776 -1.21642068922854e-15,-5.98053613748314 -4.13917310096007 -1.21197092384432e-15,-5.99512810051965 -4.06975647374412 -1.20648845742638e-15,-6 -4 -1.2e-15,-5.99512810051965 -3.93024352625587 -1.19253716267755e-15,
    # -5.98053613748314 -3.86082689903993 -1.18413630365231e-15,-5.95629520146761 -3.79208830918224 -1.17483835106499e-15,-5.92252339187664 -3.724362644183 -1.16468860360596e-15,-5.87938524157182 -3.65797985667433 -1.15373650982461e-15,-5.8270909152852 -3.5932633569242 -1.14203542722094e-15,-5.76589518571785 -3.53052843721411 -1.1296423622932e-15,-5.69609619231285 -3.4700807357668 -1.11661769280796e-15,-5.61803398874989 -3.41221474770753 -1.10302487364574e-15,-5.53208888623796 -3.35721239031346 -1.08893012765514e-15,-5.4386796006773 -3.305341629541 -1.07440212302183e-15,-5.33826121271772 -3.25685517452261 -1.05951163872403e-15,-5.23132295065132 -3.21198924639328 -1.04433121970446e-15,-5.11838580694149 -3.17096242744496 -1.02893482343865e-15,-5.0 -3.13397459621556 -1.01339745962156e-15,-4.87674229357815 -3.10120595370083 -9.97794824727899e-16,-4.74921318683182 -3.07281614543321 -9.82202933226504e-16,-4.61803398874989 -3.04894348370485 -9.66697747245474e-16,-4.48384379119934 -3.029704273724 -9.51354806492334e-16,-4.34729635533386 -3.01519224698779 -9.36248860232165e-16,-4.20905692653531 -3.00547810463173 -9.21453503116703e-16,-4.069798993405 -3.0006091729809 -9.07040816638591e-16,-3.930201006595 -3.0006091729809 -8.9308101795759e-16,-3.79094307346469 -3.00547810463173 -8.79642117809642e-16,-3.65270364466614 -3.01519224698779 -8.66789589165393e-16,-3.51615620880066 -3.029704273724 -8.54586048252467e-16,-3.38196601125011 -3.04894348370485 -8.43090949495495e-16,-3.25078681316818 -3.07281614543321 -8.32360295860139e-16,-3.12325770642185 -3.10120595370083 -8.22446366012268e-16,-3.0 -3.13397459621556 -8.13397459621556e-16,-2.88161419305851 -3.17096242744496 -8.05257662050347e-16,-2.76867704934868 -3.21198924639328 -7.98066629574196e-16,-2.66173878728228 -3.25685517452261 -7.91859396180489e-16,-2.5613203993227 -3.305341629541 -7.8666620288637e-16,-2.46791111376204 -3.35721239031346 -7.82512350407551e-16,-2.38196601125011 -3.41221474770753 -7.79418075895763e-16,-2.30390380768715 -3.4700807357668 -7.77398454345394e-16,-2.23410481428215 -3.53052843721411 -7.76463325149626e-16,-2.1729090847148 -3.5932633569242 -7.766172441639e-16,-2.12061475842818 -3.65797985667433 -7.77859461510252e-16,-2.07747660812336 -3.724362644183 -7.80183925230636e-16,-2.04370479853239 -3.79208830918224 -7.83579310771463e-16,-2.01946386251686 -3.86082689903993 -7.88029076155679e-16,-2.00487189948035 -3.93024352625587 -7.93511542573623e-16,-2 -4 -8e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-2 -4 -8e-16,-2.00487189948035 -4.06975647374412 -8.07462837322448e-16,-2.01946386251686 -4.13917310096007 -8.15863696347693e-16,-2.04370479853239 -4.20791169081776 -8.25161648935015e-16,-2.07747660812336 -4.275637355817 -8.35311396394036e-16,-2.12061475842818 -4.34202014332567 -8.46263490175385e-16,-2.1729090847148 -4.4067366430758 -8.5796457277906e-16,-2.23410481428215 -4.46947156278589 -8.70357637706804e-16,-2.30390380768715 -4.52991926423321 -8.83382307192036e-16,-2.38196601125011 -4.58778525229247 -8.96975126354258e-16,-2.46791111376204 -4.64278760968654 -9.11069872344859e-16,-2.5613203993227 -4.694658370459 -9.2559787697817e-16,-2.66173878728228 -4.74314482547739 -9.40488361275968e-16,-2.76867704934868 -4.78801075360672 -9.5566878029554e-16,-2.88161419305851 -4.82903757255504 -9.71065176561355e-16,-3.0 -4.86602540378444 -9.86602540378444e-16,-3.12325770642185 -4.89879404629917 -1.0022051752721e-15,-3.25078681316818 -4.92718385456679 -1.0177970667735e-15,-3.38196601125011 -4.95105651629515 -1.03330225275453e-15,-3.51615620880067 -4.970295726276 -1.04864519350767e-15,-3.65270364466614 -4.98480775301221 -1.06375113976783e-15,-3.79094307346469 -4.99452189536827 -1.0785464968833e-15,-3.930201006595 -4.9993908270191 -1.09295918336141e-15,-4.069798993405 -4.9993908270191 -1.10691898204241e-15,-4.20905692653531 -4.99452189536827 -1.12035788219036e-15,-4.34729635533386 -4.98480775301221 -1.13321041083461e-15,-4.48384379119934 -4.970295726276 -1.14541395174753e-15,-4.61803398874989 -4.95105651629515 -1.1569090505045e-15,-4.74921318683182 -4.92718385456679 -1.16763970413986e-15,-4.87674229357816 -4.89879404629917 -1.17755363398773e-15,-5.0 -4.86602540378444 -1.18660254037844e-15,-5.11838580694149 -4.82903757255504 -1.19474233794965e-15,-5.23132295065132 -4.78801075360672 -1.2019333704258e-15,-5.33826121271772 -4.74314482547739 -1.20814060381951e-15,-5.4386796006773 -4.694658370459 -1.21333379711363e-15,-5.53208888623796 -4.64278760968654 -1.21748764959245e-15,-5.61803398874989 -4.58778525229247 -1.22058192410424e-15,-5.69609619231285 -4.5299192642332 -1.22260154565461e-15,-5.76589518571785 -4.46947156278589 -1.22353667485037e-15,-5.8270909152852 -4.4067366430758 -1.2233827558361e-15,-5.87938524157182 -4.34202014332567 -1.22214053848975e-15,-5.92252339187664 -4.275637355817 -1.21981607476936e-15,-5.95629520146761 -4.20791169081776 -1.21642068922854e-15,-5.98053613748314 -4.13917310096007 -1.21197092384432e-15,-5.99512810051965 -4.06975647374412 -1.20648845742638e-15,-6 -4 -1.2e-15,-5.99512810051965 -3.93024352625587 -1.19253716267755e-15,"
        + "-5.98053613748314 -3.86082689903993 -1.18413630365231e-15,-5.95629520146761 -3.79208830918224 -1.17483835106499e-15,-5.92252339187664 -3.724362644183 -1.16468860360596e-15,-5.87938524157182 -3.65797985667433 -1.15373650982461e-15,-5.8270909152852 -3.5932633569242 -1.14203542722094e-15,-5.76589518571785 -3.53052843721411 -1.1296423622932e-15,-5.69609619231285 -3.4700807357668 -1.11661769280796e-15,-5.61803398874989 -3.41221474770753 -1.10302487364574e-15,-5.53208888623796 -3.35721239031346 -1.08893012765514e-15,-5.4386796006773 -3.305341629541 -1.07440212302183e-15,-5.33826121271772 -3.25685517452261 -1.05951163872403e-15,-5.23132295065132 -3.21198924639328 -1.04433121970446e-15,-5.11838580694149 -3.17096242744496 -1.02893482343865e-15,-5.0 -3.13397459621556 -1.01339745962156e-15,-4.87674229357815 -3.10120595370083 -9.97794824727899e-16,-4.74921318683182 -3.07281614543321 -9.82202933226504e-16,-4.61803398874989 -3.04894348370485 -9.66697747245474e-16,-4.48384379119934 -3.029704273724 -9.51354806492334e-16,-4.34729635533386 -3.01519224698779 -9.36248860232165e-16,-4.20905692653531 -3.00547810463173 -9.21453503116703e-16,-4.069798993405 -3.0006091729809 -9.07040816638591e-16,-3.930201006595 -3.0006091729809 -8.9308101795759e-16,-3.79094307346469 -3.00547810463173 -8.79642117809642e-16,-3.65270364466614 -3.01519224698779 -8.66789589165393e-16,-3.51615620880066 -3.029704273724 -8.54586048252467e-16,-3.38196601125011 -3.04894348370485 -8.43090949495495e-16,-3.25078681316818 -3.07281614543321 -8.32360295860139e-16,-3.12325770642185 -3.10120595370083 -8.22446366012268e-16,-3.0 -3.13397459621556 -8.13397459621556e-16,-2.88161419305851 -3.17096242744496 -8.05257662050347e-16,-2.76867704934868 -3.21198924639328 -7.98066629574196e-16,-2.66173878728228 -3.25685517452261 -7.91859396180489e-16,-2.5613203993227 -3.305341629541 -7.8666620288637e-16,-2.46791111376204 -3.35721239031346 -7.82512350407551e-16,-2.38196601125011 -3.41221474770753 -7.79418075895763e-16,-2.30390380768715 -3.4700807357668 -7.77398454345394e-16,-2.23410481428215 -3.53052843721411 -7.76463325149626e-16,-2.1729090847148 -3.5932633569242 -7.766172441639e-16,-2.12061475842818 -3.65797985667433 -7.77859461510252e-16,-2.07747660812336 -3.724362644183 -7.80183925230636e-16,-2.04370479853239 -3.79208830918224 -7.83579310771463e-16,-2.01946386251686 -3.86082689903993 -7.88029076155679e-16,-2.00487189948035 -3.93024352625587 -7.93511542573623e-16,-2 -4 -8e-16)",
    )

    # OGRFeature(entities):32
    #   EntityHandle (String) = 21F
    #   LINESTRING Z (-2 -2 0,-1.96657794502105 -2.03582232791524 0,-1.93571660708646 -2.07387296203834 0,-1.90756413746468 -2.11396923855472 0,-1.88225568337755 -2.15591867344963 0,-1.85991273921989 -2.19951988653655 0,-1.84064256332004 -2.24456356819194 0,-1.8245376630414 -2.29083348415575 0,-1.81167535069652 -2.33810751357387 0,-1.80211737240583 -2.38615871529951 0,-1.79590961168258 -2.43475641733454 0,-1.79308186916688 -2.48366732418105 0,-1.79364771956639 -2.53265663678705 0,-1.79760444649032 -2.58148917971011 0,-1.80493305548955 -2.62993053008786 0,-1.81559836524041 -2.67774814299567 0,-1.8295491764342 -2.72471246778926 0,-1.84671851756181 -2.7705980500731 0,-1.86702396641357 -2.81518461400453 0,-1.89036804575079 -2.85825811973811 0,-1.91663869124976 -2.89961179093367 0,-1.94570978947168 -2.93904710739563 0,-1.97744178327594 -2.97637475807832 0,-2.01168234177068 -3.01141554988232 0,-2.04826709158413 -3.04400126787917 0,-2.08702040594658 -3.07397548283483 0,-2.12775624779472 -3.10119430215541 0,-2.1702790628511 -3.12552706065018 0,-2.2143847183914 -3.14685694779575 0,-2.25986148319297 -3.16508156849045 0,-2.30649104396024 -3.18011343460661 0,-2.35404955334774 -3.1918803849814 0,-2.40230870454951 -3.20032593182975 0,-2.45103682729644 -3.2054095319166 0,-2.5 -3.20710678118655 0,-2.54896317270356 -3.2054095319166 0,-2.59769129545049 -3.20032593182975 0,-2.64595044665226 -3.1918803849814 0,-2.69350895603976 -3.18011343460661 0,-2.74013851680703 -3.16508156849045 0,-2.7856152816086 -3.14685694779575 0,-2.8297209371489 -3.12552706065018 0,-2.87224375220528 -3.10119430215541 0,-2.91297959405342 -3.07397548283483 0,-2.95173290841587 -3.04400126787917 0,-2.98831765822932 -3.01141554988232 0,-3.02255821672406 -2.97637475807832 0,-3.05429021052832 -2.93904710739563 0,-3.08336130875024 -2.89961179093367 0,-3.10963195424921 -2.85825811973811 0,-3.13297603358643 -2.81518461400453 0,-3.15328148243819 -2.7705980500731 0,-3.1704508235658 -2.72471246778926 0,-3.18440163475959 -2.67774814299567 0,-3.19506694451045 -2.62993053008786 0,-3.20239555350968 -2.58148917971011 0,-3.20635228043361 -2.53265663678705 0,-3.20691813083312 -2.48366732418105 0,-3.20409038831742 -2.43475641733454 0,-3.19788262759417 -2.38615871529951 0,-3.18832464930348 -2.33810751357387 0,-3.1754623369586 -2.29083348415575 0,-3.15935743667996 -2.24456356819194 0,-3.14008726078011 -2.19951988653655 0,-3.11774431662245 -2.15591867344963 0,-3.09243586253532 -2.11396923855472 0,-3.06428339291354 -2.07387296203834 0,-3.03342205497895 -2.03582232791524 0,-3 -2 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-2 -2 0,-1.96657794502105 -2.03582232791524 0,-1.93571660708646 -2.07387296203834 0,-1.90756413746468 -2.11396923855472 0,-1.88225568337755 -2.15591867344963 0,-1.85991273921989 -2.19951988653655 0,-1.84064256332004 -2.24456356819194 0,-1.8245376630414 -2.29083348415575 0,-1.81167535069652 -2.33810751357387 0,-1.80211737240583 -2.38615871529951 0,-1.79590961168258 -2.43475641733454 0,-1.79308186916688 -2.48366732418105 0,-1.79364771956639 -2.53265663678705 0,-1.79760444649032 -2.58148917971011 0,-1.80493305548955 -2.62993053008786 0,-1.81559836524041 -2.67774814299567 0,-1.8295491764342 -2.72471246778926 0,-1.84671851756181 -2.7705980500731 0,-1.86702396641357 -2.81518461400453 0,-1.89036804575079 -2.85825811973811 0,-1.91663869124976 -2.89961179093367 0,-1.94570978947168 -2.93904710739563 0,-1.97744178327594 -2.97637475807832 0,-2.01168234177068 -3.01141554988232 0,-2.04826709158413 -3.04400126787917 0,-2.08702040594658 -3.07397548283483 0,-2.12775624779472 -3.10119430215541 0,-2.1702790628511 -3.12552706065018 0,-2.2143847183914 -3.14685694779575 0,-2.25986148319297 -3.16508156849045 0,-2.30649104396024 -3.18011343460661 0,-2.35404955334774 -3.1918803849814 0,-2.40230870454951 -3.20032593182975 0,-2.45103682729644 -3.2054095319166 0,-2.5 -3.20710678118655 0,-2.54896317270356 -3.2054095319166 0,-2.59769129545049 -3.20032593182975 0,-2.64595044665226 -3.1918803849814 0,-2.69350895603976 -3.18011343460661 0,-2.74013851680703 -3.16508156849045 0,-2.7856152816086 -3.14685694779575 0,-2.8297209371489 -3.12552706065018 0,-2.87224375220528 -3.10119430215541 0,-2.91297959405342 -3.07397548283483 0,-2.95173290841587 -3.04400126787917 0,-2.98831765822932 -3.01141554988232 0,-3.02255821672406 -2.97637475807832 0,-3.05429021052832 -2.93904710739563 0,-3.08336130875024 -2.89961179093367 0,-3.10963195424921 -2.85825811973811 0,-3.13297603358643 -2.81518461400453 0,-3.15328148243819 -2.7705980500731 0,-3.1704508235658 -2.72471246778926 0,-3.18440163475959 -2.67774814299567 0,-3.19506694451045 -2.62993053008786 0,-3.20239555350968 -2.58148917971011 0,-3.20635228043361 -2.53265663678705 0,-3.20691813083312 -2.48366732418105 0,-3.20409038831742 -2.43475641733454 0,-3.19788262759417 -2.38615871529951 0,-3.18832464930348 -2.33810751357387 0,-3.1754623369586 -2.29083348415575 0,-3.15935743667996 -2.24456356819194 0,-3.14008726078011 -2.19951988653655 0,-3.11774431662245 -2.15591867344963 0,-3.09243586253532 -2.11396923855472 0,-3.06428339291354 -2.07387296203834 0,-3.03342205497895 -2.03582232791524 0,-3 -2 0)",
    )

    # OGRFeature(entities):33
    #   EntityHandle (String) = 220
    #   POLYGON Z ((-1 -2 -4e-16,-1 -3 -5e-16,-2 -3 -6e-16,-2 -2 -5e-16,-1 -2 -4e-16))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((-1 -2 -4e-16,-1 -3 -5e-16,-2 -3 -6e-16,-2 -2 -5e-16,-1 -2 -4e-16))",
    )

    # OGRFeature(entities):34
    #   EntityHandle (String) = 221
    #   POLYGON ((-3 -4,-4 -4,-4 -3,-3 -3,-3 -4))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POLYGON ((-3 -4,-4 -4,-4 -3,-3 -3,-3 -4))")

    # OGRFeature(entities):35
    #   EntityHandle (String) = 223
    #   POLYGON Z ((-8 -8 -1.6E-15,-9 -8 -1.7E-15,-9 -9 -1.8E-15,-8 -9 -1.7E-15,-8 -8 -1.6E-15))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((-8 -8 -1.6E-15,-9 -8 -1.7E-15,-9 -9 -1.8E-15,-8 -9 -1.7E-15,-8 -8 -1.6E-15))",
    )

    # OGRFeature(entities):36
    #   EntityHandle (String) = 224
    #   LINESTRING Z (-2 -2 -0.0,-2.15384615384615 -2.15384615384615 -0.0,-2.30769230769231 -2.30769230769231 -0.0,-2.46153846153846 -2.46153846153846 -0.0,-2.61538461538461 -2.61538461538461 -0.0,-2.76923076923077 -2.76923076923077 -0.0,-2.92307692307692 -2.92307692307692 -0.0,-3.07692307692308 -3.07692307692308 -0.0,-3.23076923076923 -3.23076923076923 -0.0,-3.38461538461538 -3.38461538461538 -0.0,-3.53846153846154 -3.53846153846154 -0.0,-3.69230769230769 -3.69230769230769 -0.0,-3.84615384615385 -3.84615384615385 -0.0,-4 -4 -0.0,-4.15384615384615 -4.15384615384615 -0.0,-4.30769230769231 -4.30769230769231 -0.0,-4.46153846153846 -4.46153846153846 -0.0,-4.61538461538462 -4.61538461538462 -0.0,-4.76923076923077 -4.76923076923077 -0.0,-4.92307692307692 -4.92307692307692 -0.0,-5.07692307692308 -5.07692307692308 -0.0,-5.23076923076923 -5.23076923076923 -0.0,-5.38461538461538 -5.38461538461538 -0.0,-5.53846153846154 -5.53846153846154 -0.0,-5.69230769230769 -5.69230769230769 -0.0,-5.84615384615385 -5.84615384615385 -0.0,-6 -6 -0.0,-6.15384615384615 -6.15384615384615 -0.0,-6.30769230769231 -6.30769230769231 -0.0,-6.46153846153846 -6.46153846153846 -0.0,-6.61538461538462 -6.61538461538462 -0.0,-6.76923076923077 -6.76923076923077 -0.0,-6.92307692307692 -6.92307692307692 -0.0,-7.07692307692308 -7.07692307692308 -0.0,-7.23076923076923 -7.23076923076923 -0.0,-7.38461538461539 -7.38461538461539 -0.0,-7.53846153846154 -7.53846153846154 -0.0,-7.69230769230769 -7.69230769230769 -0.0,-7.84615384615385 -7.84615384615385 -0.0,-8 -8 -0.0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-2 -2 -0.0,-2.15384615384615 -2.15384615384615 -0.0,-2.30769230769231 -2.30769230769231 -0.0,-2.46153846153846 -2.46153846153846 -0.0,-2.61538461538461 -2.61538461538461 -0.0,-2.76923076923077 -2.76923076923077 -0.0,-2.92307692307692 -2.92307692307692 -0.0,-3.07692307692308 -3.07692307692308 -0.0,-3.23076923076923 -3.23076923076923 -0.0,-3.38461538461538 -3.38461538461538 -0.0,-3.53846153846154 -3.53846153846154 -0.0,-3.69230769230769 -3.69230769230769 -0.0,-3.84615384615385 -3.84615384615385 -0.0,-4 -4 -0.0,-4.15384615384615 -4.15384615384615 -0.0,-4.30769230769231 -4.30769230769231 -0.0,-4.46153846153846 -4.46153846153846 -0.0,-4.61538461538462 -4.61538461538462 -0.0,-4.76923076923077 -4.76923076923077 -0.0,-4.92307692307692 -4.92307692307692 -0.0,-5.07692307692308 -5.07692307692308 -0.0,-5.23076923076923 -5.23076923076923 -0.0,-5.38461538461538 -5.38461538461538 -0.0,-5.53846153846154 -5.53846153846154 -0.0,-5.69230769230769 -5.69230769230769 -0.0,-5.84615384615385 -5.84615384615385 -0.0,-6 -6 -0.0,-6.15384615384615 -6.15384615384615 -0.0,-6.30769230769231 -6.30769230769231 -0.0,-6.46153846153846 -6.46153846153846 -0.0,-6.61538461538462 -6.61538461538462 -0.0,-6.76923076923077 -6.76923076923077 -0.0,-6.92307692307692 -6.92307692307692 -0.0,-7.07692307692308 -7.07692307692308 -0.0,-7.23076923076923 -7.23076923076923 -0.0,-7.38461538461539 -7.38461538461539 -0.0,-7.53846153846154 -7.53846153846154 -0.0,-7.69230769230769 -7.69230769230769 -0.0,-7.84615384615385 -7.84615384615385 -0.0,-8 -8 -0.0)",
    )

    # OGRFeature(entities):37
    #   EntityHandle (String) = 225
    #   LINESTRING Z (-8 -1 -0.0,-7.62837370825536 -0.987348067229724 -0.0,-7.25775889681215 -0.975707614760869 -0.0,-6.88916704597178 -0.966090122894857 -0.0,-6.52360963603567 -0.959507071933107 -0.0,-6.16209814730525 -0.956969942177043 -0.0,-5.80564406008193 -0.959490213928084 -0.0,-5.45525885466714 -0.968079367487651 -0.0,-5.11195401136229 -0.983748883157167 -0.0,-4.77674101046882 -1.00751024123805 -0.0,-4.45063133228814 -1.04037492203173 -0.0,-4.13463645712167 -1.08335440583961 -0.0,-3.82976786527082 -1.13746017296313 -0.0,-3.53703703703704 -1.2037037037037 -0.0,-3.25745545272173 -1.28309647836275 -0.0,-2.99203459262631 -1.37664997724169 -0.0,-2.74178593705221 -1.48537568064195 -0.0,-2.50772096630085 -1.61028506886495 -0.0,-2.29085116067365 -1.75238962221211 -0.0,-2.09218800047203 -1.91270082098484 -0.0,-1.91270082098485 -2.09218800047202 -0.0,-1.75238962221211 -2.29085116067364 -0.0,-1.61028506886495 -2.50772096630085 -0.0,-1.48537568064195 -2.74178593705221 -0.0,-1.37664997724169 -2.99203459262631 -0.0,-1.28309647836275 -3.25745545272172 -0.0,-1.2037037037037 -3.53703703703703 -0.0,-1.13746017296313 -3.82976786527082 -0.0,-1.08335440583961 -4.13463645712166 -0.0,-1.04037492203173 -4.45063133228814 -0.0,-1.00751024123805 -4.77674101046882 -0.0,-0.983748883157167 -5.11195401136229 -0.0,-0.968079367487652 -5.45525885466714 -0.0,-0.959490213928084 -5.80564406008193 -0.0,-0.956969942177043 -6.16209814730525 -0.0,-0.959507071933108 -6.52360963603567 -0.0,-0.966090122894857 -6.88916704597178 -0.0,-0.975707614760869 -7.25775889681216 -0.0,-0.987348067229724 -7.62837370825537 -0.0,-1 -8 -0.0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-8 -1 -0.0,-7.62837370825536 -0.987348067229724 -0.0,-7.25775889681215 -0.975707614760869 -0.0,-6.88916704597178 -0.966090122894857 -0.0,-6.52360963603567 -0.959507071933107 -0.0,-6.16209814730525 -0.956969942177043 -0.0,-5.80564406008193 -0.959490213928084 -0.0,-5.45525885466714 -0.968079367487651 -0.0,-5.11195401136229 -0.983748883157167 -0.0,-4.77674101046882 -1.00751024123805 -0.0,-4.45063133228814 -1.04037492203173 -0.0,-4.13463645712167 -1.08335440583961 -0.0,-3.82976786527082 -1.13746017296313 -0.0,-3.53703703703704 -1.2037037037037 -0.0,-3.25745545272173 -1.28309647836275 -0.0,-2.99203459262631 -1.37664997724169 -0.0,-2.74178593705221 -1.48537568064195 -0.0,-2.50772096630085 -1.61028506886495 -0.0,-2.29085116067365 -1.75238962221211 -0.0,-2.09218800047203 -1.91270082098484 -0.0,-1.91270082098485 -2.09218800047202 -0.0,-1.75238962221211 -2.29085116067364 -0.0,-1.61028506886495 -2.50772096630085 -0.0,-1.48537568064195 -2.74178593705221 -0.0,-1.37664997724169 -2.99203459262631 -0.0,-1.28309647836275 -3.25745545272172 -0.0,-1.2037037037037 -3.53703703703703 -0.0,-1.13746017296313 -3.82976786527082 -0.0,-1.08335440583961 -4.13463645712166 -0.0,-1.04037492203173 -4.45063133228814 -0.0,-1.00751024123805 -4.77674101046882 -0.0,-0.983748883157167 -5.11195401136229 -0.0,-0.968079367487652 -5.45525885466714 -0.0,-0.959490213928084 -5.80564406008193 -0.0,-0.956969942177043 -6.16209814730525 -0.0,-0.959507071933108 -6.52360963603567 -0.0,-0.966090122894857 -6.88916704597178 -0.0,-0.975707614760869 -7.25775889681216 -0.0,-0.987348067229724 -7.62837370825537 -0.0,-1 -8 -0.0)",
    )

    # OGRFeature(entities):38
    #   EntityHandle (String) = 226
    #   POINT Z (-7 -7 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (-7 -7 0)")

    # OGRFeature(entities):39
    #   EntityHandle (String) = 227
    #   POINT Z (4 -4 -5e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (4 -4 -5e-16)")

    # OGRFeature(entities):40
    #   EntityHandle (String) = 228
    #   LINESTRING Z (0 0 0,1 -1 -1e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (0 0 0,1 -1 -1e-16)")

    # OGRFeature(entities):41
    #   EntityHandle (String) = 229
    #   LINESTRING Z (1 -1 -1E-16,2 -1 -1E-16,1 -2 -2E-16,1 -1 -1E-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (1 -1 -1E-16,2 -1 -1E-16,1 -2 -2E-16,1 -1 -1E-16)"
    )

    # OGRFeature(entities):42
    #   EntityHandle (String) = 22A
    #   LINESTRING Z (1 -1 -1e-16,1 -2 -2e-16,2 -2 -2e-16,1 -1 -1e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (1 -1 -1e-16,1 -2 -2e-16,2 -2 -2e-16,1 -1 -1e-16)"
    )

    # OGRFeature(entities):43
    #   EntityHandle (String) = 22F
    #   LINESTRING Z (2 -4 -4e-16,2.00487189948035 -4.13951294748825 -4.13951294748825e-16,2.01946386251686 -4.27834620192013 -4.27834620192013e-16,2.04370479853239 -4.41582338163552 -4.41582338163552e-16,2.07747660812336 -4.551274711634 -4.551274711634e-16,2.12061475842818 -4.68404028665134 -4.68404028665134e-16,2.1729090847148 -4.8134732861516 -4.8134732861516e-16,2.23410481428215 -4.93894312557178 -4.93894312557178e-16,2.30390380768715 -5.05983852846641 -5.05983852846641e-16,2.38196601125011 -5.17557050458495 -5.17557050458495e-16,2.46791111376204 -5.28557521937308 -5.28557521937308e-16,2.5613203993227 -5.38931674091799 -5.38931674091799e-16,2.66173878728228 -5.48628965095479 -5.48628965095479e-16,2.76867704934868 -5.57602150721344 -5.57602150721344e-16,2.88161419305851 -5.65807514511008 -5.65807514511008e-16,3.0 -5.73205080756888 -5.73205080756888e-16,3.12325770642185 -5.79758809259833 -5.79758809259833e-16,3.25078681316818 -5.85436770913357 -5.85436770913357e-16,3.38196601125011 -5.90211303259031 -5.90211303259031e-16,3.51615620880066 -5.94059145255199 -5.94059145255199e-16,3.65270364466614 -5.96961550602442 -5.96961550602442e-16,3.79094307346469 -5.98904379073655 -5.98904379073655e-16,3.930201006595 -5.99878165403819 -5.99878165403819e-16,4.069798993405 -5.99878165403819 -5.99878165403819e-16,4.20905692653531 -5.98904379073655 -5.98904379073655e-16,4.34729635533386 -5.96961550602442 -5.96961550602442e-16,4.48384379119934 -5.94059145255199 -5.94059145255199e-16,4.61803398874989 -5.90211303259031 -5.90211303259031e-16,4.74921318683182 -5.85436770913357 -5.85436770913357e-16,4.87674229357815 -5.79758809259833 -5.79758809259833e-16,5.0 -5.73205080756888 -5.73205080756888e-16,5.11838580694149 -5.65807514511008 -5.65807514511008e-16,5.23132295065132 -5.57602150721344 -5.57602150721344e-16,5.33826121271772 -5.48628965095479 -5.48628965095479e-16,5.4386796006773 -5.38931674091799 -5.38931674091799e-16,5.53208888623796 -5.28557521937308 -5.28557521937308e-16,5.61803398874989 -5.17557050458495 -5.17557050458495e-16,5.69609619231285 -5.05983852846641 -5.05983852846641e-16,5.76589518571785 -4.93894312557178 -4.93894312557178e-16,5.8270909152852 -4.8134732861516 -4.8134732861516e-16,5.87938524157182 -4.68404028665134 -4.68404028665134e-16,5.92252339187664 -4.551274711634 -4.551274711634e-16,5.95629520146761 -4.41582338163552 -4.41582338163552e-16,5.98053613748314 -4.27834620192013 -4.27834620192013e-16,5.99512810051965 -4.13951294748825 -4.13951294748825e-16,6 -4 -4e-16,5.99512810051965 -3.86048705251175 -3.86048705251175e-16,5.98053613748314 -3.72165379807987 -3.72165379807987e-16,5.95629520146761 -3.58417661836448 -3.58417661836448e-16,5.92252339187664 -3.448725288366 -3.448725288366e-16,
    # 5.87938524157182 -3.31595971334866 -3.31595971334866e-16,5.8270909152852 -3.1865267138484 -3.1865267138484e-16,5.76589518571785 -3.06105687442822 -3.06105687442822e-16,5.69609619231285 -2.94016147153359 -2.94016147153359e-16,5.61803398874989 -2.82442949541505 -2.82442949541505e-16,5.53208888623796 -2.71442478062692 -2.71442478062692e-16,5.4386796006773 -2.61068325908201 -2.61068325908201e-16,5.33826121271772 -2.51371034904521 -2.51371034904521e-16,5.23132295065132 -2.42397849278656 -2.42397849278656e-16,5.11838580694149 -2.34192485488992 -2.34192485488992e-16,5.0 -2.26794919243112 -2.26794919243112e-16,4.87674229357816 -2.20241190740167 -2.20241190740167e-16,4.74921318683182 -2.14563229086643 -2.14563229086643e-16,4.61803398874989 -2.09788696740969 -2.09788696740969e-16,4.48384379119934 -2.05940854744801 -2.05940854744801e-16,4.34729635533386 -2.03038449397558 -2.03038449397558e-16,4.20905692653531 -2.01095620926345 -2.01095620926345e-16,4.069798993405 -2.00121834596181 -2.00121834596181e-16,3.930201006595 -2.00121834596181 -2.00121834596181e-16,3.79094307346469 -2.01095620926345 -2.01095620926345e-16,3.65270364466614 -2.03038449397558 -2.03038449397558e-16,3.51615620880067 -2.05940854744801 -2.05940854744801e-16,3.38196601125011 -2.09788696740969 -2.09788696740969e-16,3.25078681316818 -2.14563229086643 -2.14563229086643e-16,3.12325770642185 -2.20241190740167 -2.20241190740167e-16,3.0 -2.26794919243112 -2.26794919243112e-16,2.88161419305851 -2.34192485488992 -2.34192485488992e-16,2.76867704934868 -2.42397849278656 -2.42397849278656e-16,2.66173878728228 -2.51371034904521 -2.51371034904521e-16,2.5613203993227 -2.610683259082 -2.610683259082e-16,2.46791111376204 -2.71442478062692 -2.71442478062692e-16,2.38196601125011 -2.82442949541505 -2.82442949541505e-16,2.30390380768715 -2.94016147153359 -2.94016147153359e-16,2.23410481428215 -3.06105687442822 -3.06105687442822e-16,2.1729090847148 -3.1865267138484 -3.1865267138484e-16,2.12061475842818 -3.31595971334866 -3.31595971334866e-16,2.07747660812336 -3.448725288366 -3.448725288366e-16,2.04370479853239 -3.58417661836448 -3.58417661836448e-16,2.01946386251686 -3.72165379807987 -3.72165379807987e-16,2.00487189948035 -3.86048705251175 -3.86048705251175e-16,2.0 -4.0 -4e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 -4 -4e-16,2.00487189948035 -4.13951294748825 -4.13951294748825e-16,2.01946386251686 -4.27834620192013 -4.27834620192013e-16,2.04370479853239 -4.41582338163552 -4.41582338163552e-16,2.07747660812336 -4.551274711634 -4.551274711634e-16,2.12061475842818 -4.68404028665134 -4.68404028665134e-16,2.1729090847148 -4.8134732861516 -4.8134732861516e-16,2.23410481428215 -4.93894312557178 -4.93894312557178e-16,2.30390380768715 -5.05983852846641 -5.05983852846641e-16,2.38196601125011 -5.17557050458495 -5.17557050458495e-16,2.46791111376204 -5.28557521937308 -5.28557521937308e-16,2.5613203993227 -5.38931674091799 -5.38931674091799e-16,2.66173878728228 -5.48628965095479 -5.48628965095479e-16,2.76867704934868 -5.57602150721344 -5.57602150721344e-16,2.88161419305851 -5.65807514511008 -5.65807514511008e-16,3.0 -5.73205080756888 -5.73205080756888e-16,3.12325770642185 -5.79758809259833 -5.79758809259833e-16,3.25078681316818 -5.85436770913357 -5.85436770913357e-16,3.38196601125011 -5.90211303259031 -5.90211303259031e-16,3.51615620880066 -5.94059145255199 -5.94059145255199e-16,3.65270364466614 -5.96961550602442 -5.96961550602442e-16,3.79094307346469 -5.98904379073655 -5.98904379073655e-16,3.930201006595 -5.99878165403819 -5.99878165403819e-16,4.069798993405 -5.99878165403819 -5.99878165403819e-16,4.20905692653531 -5.98904379073655 -5.98904379073655e-16,4.34729635533386 -5.96961550602442 -5.96961550602442e-16,4.48384379119934 -5.94059145255199 -5.94059145255199e-16,4.61803398874989 -5.90211303259031 -5.90211303259031e-16,4.74921318683182 -5.85436770913357 -5.85436770913357e-16,4.87674229357815 -5.79758809259833 -5.79758809259833e-16,5.0 -5.73205080756888 -5.73205080756888e-16,5.11838580694149 -5.65807514511008 -5.65807514511008e-16,5.23132295065132 -5.57602150721344 -5.57602150721344e-16,5.33826121271772 -5.48628965095479 -5.48628965095479e-16,5.4386796006773 -5.38931674091799 -5.38931674091799e-16,5.53208888623796 -5.28557521937308 -5.28557521937308e-16,5.61803398874989 -5.17557050458495 -5.17557050458495e-16,5.69609619231285 -5.05983852846641 -5.05983852846641e-16,5.76589518571785 -4.93894312557178 -4.93894312557178e-16,5.8270909152852 -4.8134732861516 -4.8134732861516e-16,5.87938524157182 -4.68404028665134 -4.68404028665134e-16,5.92252339187664 -4.551274711634 -4.551274711634e-16,5.95629520146761 -4.41582338163552 -4.41582338163552e-16,5.98053613748314 -4.27834620192013 -4.27834620192013e-16,5.99512810051965 -4.13951294748825 -4.13951294748825e-16,6 -4 -4e-16,5.99512810051965 -3.86048705251175 -3.86048705251175e-16,5.98053613748314 -3.72165379807987 -3.72165379807987e-16,5.95629520146761 -3.58417661836448 -3.58417661836448e-16,5.92252339187664 -3.448725288366 -3.448725288366e-16,5.87938524157182 -3.31595971334866 -3.31595971334866e-16,5.8270909152852 -3.1865267138484 -3.1865267138484e-16,5.76589518571785 -3.06105687442822 -3.06105687442822e-16,5.69609619231285 -2.94016147153359 -2.94016147153359e-16,5.61803398874989 -2.82442949541505 -2.82442949541505e-16,5.53208888623796 -2.71442478062692 -2.71442478062692e-16,5.4386796006773 -2.61068325908201 -2.61068325908201e-16,5.33826121271772 -2.51371034904521 -2.51371034904521e-16,"
        + "5.23132295065132 -2.42397849278656 -2.42397849278656e-16,5.11838580694149 -2.34192485488992 -2.34192485488992e-16,5.0 -2.26794919243112 -2.26794919243112e-16,4.87674229357816 -2.20241190740167 -2.20241190740167e-16,4.74921318683182 -2.14563229086643 -2.14563229086643e-16,4.61803398874989 -2.09788696740969 -2.09788696740969e-16,4.48384379119934 -2.05940854744801 -2.05940854744801e-16,4.34729635533386 -2.03038449397558 -2.03038449397558e-16,4.20905692653531 -2.01095620926345 -2.01095620926345e-16,4.069798993405 -2.00121834596181 -2.00121834596181e-16,3.930201006595 -2.00121834596181 -2.00121834596181e-16,3.79094307346469 -2.01095620926345 -2.01095620926345e-16,3.65270364466614 -2.03038449397558 -2.03038449397558e-16,3.51615620880067 -2.05940854744801 -2.05940854744801e-16,3.38196601125011 -2.09788696740969 -2.09788696740969e-16,3.25078681316818 -2.14563229086643 -2.14563229086643e-16,3.12325770642185 -2.20241190740167 -2.20241190740167e-16,3.0 -2.26794919243112 -2.26794919243112e-16,2.88161419305851 -2.34192485488992 -2.34192485488992e-16,2.76867704934868 -2.42397849278656 -2.42397849278656e-16,2.66173878728228 -2.51371034904521 -2.51371034904521e-16,2.5613203993227 -2.610683259082 -2.610683259082e-16,2.46791111376204 -2.71442478062692 -2.71442478062692e-16,2.38196601125011 -2.82442949541505 -2.82442949541505e-16,2.30390380768715 -2.94016147153359 -2.94016147153359e-16,2.23410481428215 -3.06105687442822 -3.06105687442822e-16,2.1729090847148 -3.1865267138484 -3.1865267138484e-16,2.12061475842818 -3.31595971334866 -3.31595971334866e-16,2.07747660812336 -3.448725288366 -3.448725288366e-16,2.04370479853239 -3.58417661836448 -3.58417661836448e-16,2.01946386251686 -3.72165379807987 -3.72165379807987e-16,2.00487189948035 -3.86048705251175 -3.86048705251175e-16,2.0 -4.0 -4e-16)",
    )

    # OGRFeature(entities):44
    #   EntityHandle (String) = 230
    #   LINESTRING Z (2 -4 -5e-16,2.00487189948035 -4.06975647374412 -5.06975647374413e-16,2.01946386251686 -4.13917310096007 -5.13917310096007e-16,2.04370479853239 -4.20791169081776 -5.20791169081776e-16,2.07747660812336 -4.275637355817 -5.275637355817e-16,2.12061475842818 -4.34202014332567 -5.34202014332567e-16,2.1729090847148 -4.4067366430758 -5.4067366430758e-16,2.23410481428215 -4.46947156278589 -5.46947156278589e-16,2.30390380768715 -4.52991926423321 -5.52991926423321e-16,2.38196601125011 -4.58778525229247 -5.58778525229247e-16,2.46791111376204 -4.64278760968654 -5.64278760968654e-16,2.5613203993227 -4.694658370459 -5.694658370459e-16,2.66173878728228 -4.74314482547739 -5.7431448254774e-16,2.76867704934868 -4.78801075360672 -5.78801075360672e-16,2.88161419305851 -4.82903757255504 -5.82903757255504e-16,3.0 -4.86602540378444 -5.86602540378444e-16,3.12325770642185 -4.89879404629917 -5.89879404629917e-16,3.25078681316818 -4.92718385456679 -5.92718385456679e-16,3.38196601125011 -4.95105651629515 -5.95105651629515e-16,3.51615620880067 -4.970295726276 -5.970295726276e-16,3.65270364466614 -4.98480775301221 -5.98480775301221e-16,3.79094307346469 -4.99452189536827 -5.99452189536827e-16,3.930201006595 -4.9993908270191 -5.9993908270191e-16,4.069798993405 -4.9993908270191 -5.9993908270191e-16,4.20905692653531 -4.99452189536827 -5.99452189536827e-16,4.34729635533386 -4.98480775301221 -5.98480775301221e-16,4.48384379119934 -4.970295726276 -5.970295726276e-16,4.61803398874989 -4.95105651629515 -5.95105651629515e-16,4.74921318683182 -4.92718385456679 -5.92718385456679e-16,4.87674229357816 -4.89879404629917 -5.89879404629917e-16,5.0 -4.86602540378444 -5.86602540378444e-16,5.11838580694149 -4.82903757255504 -5.82903757255504e-16,5.23132295065132 -4.78801075360672 -5.78801075360672e-16,5.33826121271772 -4.74314482547739 -5.74314482547739e-16,5.4386796006773 -4.694658370459 -5.694658370459e-16,5.53208888623796 -4.64278760968654 -5.64278760968654e-16,5.61803398874989 -4.58778525229247 -5.58778525229247e-16,5.69609619231285 -4.5299192642332 -5.52991926423321e-16,5.76589518571785 -4.46947156278589 -5.46947156278589e-16,5.8270909152852 -4.4067366430758 -5.4067366430758e-16,5.87938524157182 -4.34202014332567 -5.34202014332567e-16,5.92252339187664 -4.275637355817 -5.275637355817e-16,5.95629520146761 -4.20791169081776 -5.20791169081776e-16,5.98053613748314 -4.13917310096007 -5.13917310096007e-16,5.99512810051965 -4.06975647374412 -5.06975647374413e-16,6 -4 -5e-16,5.99512810051965 -3.93024352625587 -4.93024352625588e-16,5.98053613748314 -3.86082689903993 -4.86082689903993e-16,5.95629520146761 -3.79208830918224 -4.79208830918224e-16,5.92252339187664 -3.724362644183 -4.724362644183e-16,5.87938524157182 -3.65797985667433 -4.65797985667433e-16,5.8270909152852 -3.5932633569242 -4.5932633569242e-16,5.76589518571785 -3.53052843721411 -4.53052843721411e-16,
    # 5.69609619231285 -3.4700807357668 -4.4700807357668e-16,5.61803398874989 -3.41221474770753 -4.41221474770753e-16,5.53208888623796 -3.35721239031346 -4.35721239031346e-16,5.4386796006773 -3.305341629541 -4.305341629541e-16,5.33826121271772 -3.25685517452261 -4.25685517452261e-16,5.23132295065132 -3.21198924639328 -4.21198924639328e-16,5.11838580694149 -3.17096242744496 -4.17096242744496e-16,5.0 -3.13397459621556 -4.13397459621556e-16,4.87674229357815 -3.10120595370083 -4.10120595370083e-16,4.74921318683182 -3.07281614543321 -4.07281614543321e-16,4.61803398874989 -3.04894348370485 -4.04894348370485e-16,4.48384379119934 -3.029704273724 -4.029704273724e-16,4.34729635533386 -3.01519224698779 -4.01519224698779e-16,4.20905692653531 -3.00547810463173 -4.00547810463173e-16,4.069798993405 -3.0006091729809 -4.0006091729809e-16,3.930201006595 -3.0006091729809 -4.0006091729809e-16,3.79094307346469 -3.00547810463173 -4.00547810463173e-16,3.65270364466614 -3.01519224698779 -4.01519224698779e-16,3.51615620880066 -3.029704273724 -4.029704273724e-16,3.38196601125011 -3.04894348370485 -4.04894348370485e-16,3.25078681316818 -3.07281614543321 -4.07281614543321e-16,3.12325770642185 -3.10120595370083 -4.10120595370083e-16,3.0 -3.13397459621556 -4.13397459621556e-16,2.88161419305851 -3.17096242744496 -4.17096242744496e-16,2.76867704934868 -3.21198924639328 -4.21198924639328e-16,2.66173878728228 -3.25685517452261 -4.25685517452261e-16,2.5613203993227 -3.305341629541 -4.305341629541e-16,2.46791111376204 -3.35721239031346 -4.35721239031346e-16,2.38196601125011 -3.41221474770753 -4.41221474770753e-16,2.30390380768715 -3.4700807357668 -4.4700807357668e-16,2.23410481428215 -3.53052843721411 -4.53052843721411e-16,2.1729090847148 -3.5932633569242 -4.5932633569242e-16,2.12061475842818 -3.65797985667433 -4.65797985667433e-16,2.07747660812336 -3.724362644183 -4.724362644183e-16,2.04370479853239 -3.79208830918224 -4.79208830918224e-16,2.01946386251686 -3.86082689903993 -4.86082689903993e-16,2.00487189948035 -3.93024352625587 -4.93024352625588e-16,2 -4 -5e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 -4 -5e-16,2.00487189948035 -4.06975647374412 -5.06975647374413e-16,2.01946386251686 -4.13917310096007 -5.13917310096007e-16,2.04370479853239 -4.20791169081776 -5.20791169081776e-16,2.07747660812336 -4.275637355817 -5.275637355817e-16,2.12061475842818 -4.34202014332567 -5.34202014332567e-16,2.1729090847148 -4.4067366430758 -5.4067366430758e-16,2.23410481428215 -4.46947156278589 -5.46947156278589e-16,2.30390380768715 -4.52991926423321 -5.52991926423321e-16,2.38196601125011 -4.58778525229247 -5.58778525229247e-16,2.46791111376204 -4.64278760968654 -5.64278760968654e-16,2.5613203993227 -4.694658370459 -5.694658370459e-16,2.66173878728228 -4.74314482547739 -5.7431448254774e-16,2.76867704934868 -4.78801075360672 -5.78801075360672e-16,2.88161419305851 -4.82903757255504 -5.82903757255504e-16,3.0 -4.86602540378444 -5.86602540378444e-16,3.12325770642185 -4.89879404629917 -5.89879404629917e-16,3.25078681316818 -4.92718385456679 -5.92718385456679e-16,3.38196601125011 -4.95105651629515 -5.95105651629515e-16,3.51615620880067 -4.970295726276 -5.970295726276e-16,3.65270364466614 -4.98480775301221 -5.98480775301221e-16,3.79094307346469 -4.99452189536827 -5.99452189536827e-16,3.930201006595 -4.9993908270191 -5.9993908270191e-16,4.069798993405 -4.9993908270191 -5.9993908270191e-16,4.20905692653531 -4.99452189536827 -5.99452189536827e-16,4.34729635533386 -4.98480775301221 -5.98480775301221e-16,4.48384379119934 -4.970295726276 -5.970295726276e-16,4.61803398874989 -4.95105651629515 -5.95105651629515e-16,4.74921318683182 -4.92718385456679 -5.92718385456679e-16,4.87674229357816 -4.89879404629917 -5.89879404629917e-16,5.0 -4.86602540378444 -5.86602540378444e-16,5.11838580694149 -4.82903757255504 -5.82903757255504e-16,5.23132295065132 -4.78801075360672 -5.78801075360672e-16,5.33826121271772 -4.74314482547739 -5.74314482547739e-16,5.4386796006773 -4.694658370459 -5.694658370459e-16,5.53208888623796 -4.64278760968654 -5.64278760968654e-16,5.61803398874989 -4.58778525229247 -5.58778525229247e-16,5.69609619231285 -4.5299192642332 -5.52991926423321e-16,5.76589518571785 -4.46947156278589 -5.46947156278589e-16,5.8270909152852 -4.4067366430758 -5.4067366430758e-16,5.87938524157182 -4.34202014332567 -5.34202014332567e-16,5.92252339187664 -4.275637355817 -5.275637355817e-16,5.95629520146761 -4.20791169081776 -5.20791169081776e-16,5.98053613748314 -4.13917310096007 -5.13917310096007e-16,5.99512810051965 -4.06975647374412 -5.06975647374413e-16,6 -4 -5e-16,5.99512810051965 -3.93024352625587 -4.93024352625588e-16,5.98053613748314 -3.86082689903993 -4.86082689903993e-16,5.95629520146761 -3.79208830918224 -4.79208830918224e-16,5.92252339187664 -3.724362644183 -4.724362644183e-16,5.87938524157182 -3.65797985667433 -4.65797985667433e-16,5.8270909152852 -3.5932633569242 -4.5932633569242e-16,5.76589518571785 -3.53052843721411 -4.53052843721411e-16,5.69609619231285 -3.4700807357668 -4.4700807357668e-16,5.61803398874989 -3.41221474770753 -4.41221474770753e-16,"
        + "5.53208888623796 -3.35721239031346 -4.35721239031346e-16,5.4386796006773 -3.305341629541 -4.305341629541e-16,5.33826121271772 -3.25685517452261 -4.25685517452261e-16,5.23132295065132 -3.21198924639328 -4.21198924639328e-16,5.11838580694149 -3.17096242744496 -4.17096242744496e-16,5.0 -3.13397459621556 -4.13397459621556e-16,4.87674229357815 -3.10120595370083 -4.10120595370083e-16,4.74921318683182 -3.07281614543321 -4.07281614543321e-16,4.61803398874989 -3.04894348370485 -4.04894348370485e-16,4.48384379119934 -3.029704273724 -4.029704273724e-16,4.34729635533386 -3.01519224698779 -4.01519224698779e-16,4.20905692653531 -3.00547810463173 -4.00547810463173e-16,4.069798993405 -3.0006091729809 -4.0006091729809e-16,3.930201006595 -3.0006091729809 -4.0006091729809e-16,3.79094307346469 -3.00547810463173 -4.00547810463173e-16,3.65270364466614 -3.01519224698779 -4.01519224698779e-16,3.51615620880066 -3.029704273724 -4.029704273724e-16,3.38196601125011 -3.04894348370485 -4.04894348370485e-16,3.25078681316818 -3.07281614543321 -4.07281614543321e-16,3.12325770642185 -3.10120595370083 -4.10120595370083e-16,3.0 -3.13397459621556 -4.13397459621556e-16,2.88161419305851 -3.17096242744496 -4.17096242744496e-16,2.76867704934868 -3.21198924639328 -4.21198924639328e-16,2.66173878728228 -3.25685517452261 -4.25685517452261e-16,2.5613203993227 -3.305341629541 -4.305341629541e-16,2.46791111376204 -3.35721239031346 -4.35721239031346e-16,2.38196601125011 -3.41221474770753 -4.41221474770753e-16,2.30390380768715 -3.4700807357668 -4.4700807357668e-16,2.23410481428215 -3.53052843721411 -4.53052843721411e-16,2.1729090847148 -3.5932633569242 -4.5932633569242e-16,2.12061475842818 -3.65797985667433 -4.65797985667433e-16,2.07747660812336 -3.724362644183 -4.724362644183e-16,2.04370479853239 -3.79208830918224 -4.79208830918224e-16,2.01946386251686 -3.86082689903993 -4.86082689903993e-16,2.00487189948035 -3.93024352625587 -4.93024352625588e-16,2 -4 -5e-16)",
    )

    # OGRFeature(entities):45
    #   EntityHandle (String) = 231
    #   LINESTRING Z (2 -2 -2e-16,1.96657794502105 -2.03582232791524 -2.03582232791524e-16,1.93571660708646 -2.07387296203834 -2.07387296203834e-16,1.90756413746468 -2.11396923855472 -2.11396923855472e-16,1.88225568337755 -2.15591867344963 -2.15591867344963e-16,1.85991273921989 -2.19951988653655 -2.19951988653655e-16,1.84064256332004 -2.24456356819194 -2.24456356819194e-16,1.8245376630414 -2.29083348415575 -2.29083348415575e-16,1.81167535069652 -2.33810751357387 -2.33810751357387e-16,1.80211737240583 -2.38615871529951 -2.38615871529951e-16,1.79590961168258 -2.43475641733454 -2.43475641733454e-16,1.79308186916688 -2.48366732418105 -2.48366732418105e-16,1.79364771956639 -2.53265663678705 -2.53265663678705e-16,1.79760444649032 -2.58148917971011 -2.58148917971011e-16,1.80493305548955 -2.62993053008786 -2.62993053008786e-16,1.81559836524041 -2.67774814299567 -2.67774814299567e-16,1.8295491764342 -2.72471246778926 -2.72471246778926e-16,1.84671851756181 -2.7705980500731 -2.7705980500731e-16,1.86702396641357 -2.81518461400453 -2.81518461400453e-16,1.89036804575079 -2.85825811973811 -2.85825811973811e-16,1.91663869124976 -2.89961179093367 -2.89961179093367e-16,1.94570978947168 -2.93904710739563 -2.93904710739563e-16,1.97744178327594 -2.97637475807832 -2.97637475807832e-16,2.01168234177068 -3.01141554988232 -3.01141554988232e-16,2.04826709158413 -3.04400126787917 -3.04400126787917e-16,2.08702040594658 -3.07397548283483 -3.07397548283483e-16,2.12775624779472 -3.10119430215541 -3.10119430215541e-16,2.1702790628511 -3.12552706065018 -3.12552706065018e-16,2.2143847183914 -3.14685694779575 -3.14685694779575e-16,2.25986148319297 -3.16508156849045 -3.16508156849045e-16,2.30649104396024 -3.18011343460661 -3.18011343460661e-16,2.35404955334774 -3.1918803849814 -3.1918803849814e-16,2.40230870454951 -3.20032593182975 -3.20032593182975e-16,2.45103682729644 -3.2054095319166 -3.2054095319166e-16,2.5 -3.20710678118655 -3.20710678118655e-16,2.54896317270356 -3.2054095319166 -3.2054095319166e-16,2.59769129545049 -3.20032593182975 -3.20032593182975e-16,2.64595044665226 -3.1918803849814 -3.1918803849814e-16,2.69350895603976 -3.18011343460661 -3.18011343460661e-16,2.74013851680703 -3.16508156849045 -3.16508156849045e-16,2.7856152816086 -3.14685694779575 -3.14685694779575e-16,2.8297209371489 -3.12552706065018 -3.12552706065018e-16,2.87224375220528 -3.10119430215541 -3.10119430215541e-16,2.91297959405342 -3.07397548283483 -3.07397548283483e-16,2.95173290841587 -3.04400126787917 -3.04400126787917e-16,2.98831765822932 -3.01141554988232 -3.01141554988232e-16,3.02255821672406 -2.97637475807832 -2.97637475807832e-16,3.05429021052832 -2.93904710739563 -2.93904710739563e-16,3.08336130875024 -2.89961179093367 -2.89961179093367e-16,3.10963195424921 -2.85825811973811 -2.85825811973811e-16,3.13297603358643 -2.81518461400453 -2.81518461400453e-16,3.15328148243819 -2.7705980500731 -2.7705980500731e-16,3.1704508235658 -2.72471246778926 -2.72471246778926e-16,3.18440163475959 -2.67774814299567 -2.67774814299567e-16,3.19506694451045 -2.62993053008786 -2.62993053008786e-16,3.20239555350968 -2.58148917971011 -2.58148917971011e-16,3.20635228043361 -2.53265663678705 -2.53265663678705e-16,3.20691813083312 -2.48366732418105 -2.48366732418105e-16,3.20409038831742 -2.43475641733454 -2.43475641733454e-16,3.19788262759417 -2.38615871529951 -2.38615871529951e-16,3.18832464930348 -2.33810751357387 -2.33810751357387e-16,3.1754623369586 -2.29083348415575 -2.29083348415575e-16,3.15935743667996 -2.24456356819194 -2.24456356819194e-16,3.14008726078011 -2.19951988653655 -2.19951988653655e-16,3.11774431662245 -2.15591867344963 -2.15591867344963e-16,3.09243586253532 -2.11396923855472 -2.11396923855471e-16,3.06428339291354 -2.07387296203834 -2.07387296203834e-16,3.03342205497895 -2.03582232791524 -2.03582232791524e-16,3 -2 -2e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 -2 -2e-16,1.96657794502105 -2.03582232791524 -2.03582232791524e-16,1.93571660708646 -2.07387296203834 -2.07387296203834e-16,1.90756413746468 -2.11396923855472 -2.11396923855472e-16,1.88225568337755 -2.15591867344963 -2.15591867344963e-16,1.85991273921989 -2.19951988653655 -2.19951988653655e-16,1.84064256332004 -2.24456356819194 -2.24456356819194e-16,1.8245376630414 -2.29083348415575 -2.29083348415575e-16,1.81167535069652 -2.33810751357387 -2.33810751357387e-16,1.80211737240583 -2.38615871529951 -2.38615871529951e-16,1.79590961168258 -2.43475641733454 -2.43475641733454e-16,1.79308186916688 -2.48366732418105 -2.48366732418105e-16,1.79364771956639 -2.53265663678705 -2.53265663678705e-16,1.79760444649032 -2.58148917971011 -2.58148917971011e-16,1.80493305548955 -2.62993053008786 -2.62993053008786e-16,1.81559836524041 -2.67774814299567 -2.67774814299567e-16,1.8295491764342 -2.72471246778926 -2.72471246778926e-16,1.84671851756181 -2.7705980500731 -2.7705980500731e-16,1.86702396641357 -2.81518461400453 -2.81518461400453e-16,1.89036804575079 -2.85825811973811 -2.85825811973811e-16,1.91663869124976 -2.89961179093367 -2.89961179093367e-16,1.94570978947168 -2.93904710739563 -2.93904710739563e-16,1.97744178327594 -2.97637475807832 -2.97637475807832e-16,2.01168234177068 -3.01141554988232 -3.01141554988232e-16,2.04826709158413 -3.04400126787917 -3.04400126787917e-16,2.08702040594658 -3.07397548283483 -3.07397548283483e-16,2.12775624779472 -3.10119430215541 -3.10119430215541e-16,2.1702790628511 -3.12552706065018 -3.12552706065018e-16,2.2143847183914 -3.14685694779575 -3.14685694779575e-16,2.25986148319297 -3.16508156849045 -3.16508156849045e-16,2.30649104396024 -3.18011343460661 -3.18011343460661e-16,2.35404955334774 -3.1918803849814 -3.1918803849814e-16,2.40230870454951 -3.20032593182975 -3.20032593182975e-16,2.45103682729644 -3.2054095319166 -3.2054095319166e-16,2.5 -3.20710678118655 -3.20710678118655e-16,2.54896317270356 -3.2054095319166 -3.2054095319166e-16,2.59769129545049 -3.20032593182975 -3.20032593182975e-16,2.64595044665226 -3.1918803849814 -3.1918803849814e-16,2.69350895603976 -3.18011343460661 -3.18011343460661e-16,2.74013851680703 -3.16508156849045 -3.16508156849045e-16,2.7856152816086 -3.14685694779575 -3.14685694779575e-16,2.8297209371489 -3.12552706065018 -3.12552706065018e-16,2.87224375220528 -3.10119430215541 -3.10119430215541e-16,2.91297959405342 -3.07397548283483 -3.07397548283483e-16,2.95173290841587 -3.04400126787917 -3.04400126787917e-16,2.98831765822932 -3.01141554988232 -3.01141554988232e-16,3.02255821672406 -2.97637475807832 -2.97637475807832e-16,3.05429021052832 -2.93904710739563 -2.93904710739563e-16,3.08336130875024 -2.89961179093367 -2.89961179093367e-16,3.10963195424921 -2.85825811973811 -2.85825811973811e-16,3.13297603358643 -2.81518461400453 -2.81518461400453e-16,3.15328148243819 -2.7705980500731 -2.7705980500731e-16,3.1704508235658 -2.72471246778926 -2.72471246778926e-16,3.18440163475959 -2.67774814299567 -2.67774814299567e-16,3.19506694451045 -2.62993053008786 -2.62993053008786e-16,3.20239555350968 -2.58148917971011 -2.58148917971011e-16,3.20635228043361 -2.53265663678705 -2.53265663678705e-16,3.20691813083312 -2.48366732418105 -2.48366732418105e-16,3.20409038831742 -2.43475641733454 -2.43475641733454e-16,3.19788262759417 -2.38615871529951 -2.38615871529951e-16,3.18832464930348 -2.33810751357387 -2.33810751357387e-16,3.1754623369586 -2.29083348415575 -2.29083348415575e-16,3.15935743667996 -2.24456356819194 -2.24456356819194e-16,3.14008726078011 -2.19951988653655 -2.19951988653655e-16,3.11774431662245 -2.15591867344963 -2.15591867344963e-16,3.09243586253532 -2.11396923855472 -2.11396923855471e-16,3.06428339291354 -2.07387296203834 -2.07387296203834e-16,3.03342205497895 -2.03582232791524 -2.03582232791524e-16,3 -2 -2e-16)",
    )

    # OGRFeature(entities):46
    #   EntityHandle (String) = 232
    #   POLYGON Z ((1 -2 -2e-16,1 -3 -4e-16,2 -3 -4e-16,2 -2 -2e-16,1 -2 -2e-16))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((1 -2 -2e-16,1 -3 -4e-16,2 -3 -4e-16,2 -2 -2e-16,1 -2 -2e-16))",
    )

    # OGRFeature(entities):47
    #   EntityHandle (String) = 233
    #   POLYGON Z ((3 -4 -4E-16,4 -4 -4E-16,4 -3 -3E-16,3 -3 -3E-16,3 -4 -4E-16))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((3 -4 -4E-16,4 -4 -4E-16,4 -3 -3E-16,3 -3 -3E-16,3 -4 -4E-16))",
    )

    # OGRFeature(entities):48
    #   EntityHandle (String) = 235
    #   POLYGON Z ((8 -8 -8E-16,9 -8 -8E-16,9 -9 -9E-16,8 -9 -9E-16,8 -8 -8E-16))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((8 -8 -8E-16,9 -8 -8E-16,9 -9 -9E-16,8 -9 -9E-16,8 -8 -8E-16))",
    )

    # OGRFeature(entities):49
    #   EntityHandle (String) = 236
    #   LINESTRING Z (2 -2 -0.0,2.15384615384615 -2.15384615384615 -0.0,2.30769230769231 -2.30769230769231 -0.0,2.46153846153846 -2.46153846153846 -0.0,2.61538461538461 -2.61538461538461 -0.0,2.76923076923077 -2.76923076923077 -0.0,2.92307692307692 -2.92307692307692 -0.0,3.07692307692308 -3.07692307692308 -0.0,3.23076923076923 -3.23076923076923 -0.0,3.38461538461538 -3.38461538461538 -0.0,3.53846153846154 -3.53846153846154 -0.0,3.69230769230769 -3.69230769230769 -0.0,3.84615384615385 -3.84615384615385 -0.0,4 -4 -0.0,4.15384615384615 -4.15384615384615 -0.0,4.30769230769231 -4.30769230769231 -0.0,4.46153846153846 -4.46153846153846 -0.0,4.61538461538462 -4.61538461538462 -0.0,4.76923076923077 -4.76923076923077 -0.0,4.92307692307692 -4.92307692307692 -0.0,5.07692307692308 -5.07692307692308 -0.0,5.23076923076923 -5.23076923076923 -0.0,5.38461538461538 -5.38461538461538 -0.0,5.53846153846154 -5.53846153846154 -0.0,5.69230769230769 -5.69230769230769 -0.0,5.84615384615385 -5.84615384615385 -0.0,6.0 -6 -0.0,6.15384615384615 -6.15384615384615 -0.0,6.30769230769231 -6.30769230769231 -0.0,6.46153846153846 -6.46153846153846 -0.0,6.61538461538462 -6.61538461538462 -0.0,6.76923076923077 -6.76923076923077 -0.0,6.92307692307692 -6.92307692307692 -0.0,7.07692307692308 -7.07692307692308 -0.0,7.23076923076923 -7.23076923076923 -0.0,7.38461538461539 -7.38461538461539 -0.0,7.53846153846154 -7.53846153846154 -0.0,7.69230769230769 -7.69230769230769 -0.0,7.84615384615385 -7.84615384615385 -0.0,8 -8 -0.0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 -2 -0.0,2.15384615384615 -2.15384615384615 -0.0,2.30769230769231 -2.30769230769231 -0.0,2.46153846153846 -2.46153846153846 -0.0,2.61538461538461 -2.61538461538461 -0.0,2.76923076923077 -2.76923076923077 -0.0,2.92307692307692 -2.92307692307692 -0.0,3.07692307692308 -3.07692307692308 -0.0,3.23076923076923 -3.23076923076923 -0.0,3.38461538461538 -3.38461538461538 -0.0,3.53846153846154 -3.53846153846154 -0.0,3.69230769230769 -3.69230769230769 -0.0,3.84615384615385 -3.84615384615385 -0.0,4 -4 -0.0,4.15384615384615 -4.15384615384615 -0.0,4.30769230769231 -4.30769230769231 -0.0,4.46153846153846 -4.46153846153846 -0.0,4.61538461538462 -4.61538461538462 -0.0,4.76923076923077 -4.76923076923077 -0.0,4.92307692307692 -4.92307692307692 -0.0,5.07692307692308 -5.07692307692308 -0.0,5.23076923076923 -5.23076923076923 -0.0,5.38461538461538 -5.38461538461538 -0.0,5.53846153846154 -5.53846153846154 -0.0,5.69230769230769 -5.69230769230769 -0.0,5.84615384615385 -5.84615384615385 -0.0,6.0 -6 -0.0,6.15384615384615 -6.15384615384615 -0.0,6.30769230769231 -6.30769230769231 -0.0,6.46153846153846 -6.46153846153846 -0.0,6.61538461538462 -6.61538461538462 -0.0,6.76923076923077 -6.76923076923077 -0.0,6.92307692307692 -6.92307692307692 -0.0,7.07692307692308 -7.07692307692308 -0.0,7.23076923076923 -7.23076923076923 -0.0,7.38461538461539 -7.38461538461539 -0.0,7.53846153846154 -7.53846153846154 -0.0,7.69230769230769 -7.69230769230769 -0.0,7.84615384615385 -7.84615384615385 -0.0,8 -8 -0.0)",
    )

    # OGRFeature(entities):50
    #   EntityHandle (String) = 237
    #   LINESTRING Z (8 -1 -0.0,7.62837370825536 -0.987348067229724 -0.0,7.25775889681215 -0.975707614760869 -0.0,6.88916704597178 -0.966090122894857 -0.0,6.52360963603567 -0.959507071933107 -0.0,6.16209814730525 -0.956969942177043 -0.0,5.80564406008193 -0.959490213928084 -0.0,5.45525885466714 -0.968079367487651 -0.0,5.11195401136229 -0.983748883157167 -0.0,4.77674101046882 -1.00751024123805 -0.0,4.45063133228814 -1.04037492203173 -0.0,4.13463645712167 -1.08335440583961 -0.0,3.82976786527082 -1.13746017296313 -0.0,3.53703703703704 -1.2037037037037 -0.0,3.25745545272173 -1.28309647836275 -0.0,2.99203459262631 -1.37664997724169 -0.0,2.74178593705221 -1.48537568064195 -0.0,2.50772096630085 -1.61028506886495 -0.0,2.29085116067365 -1.75238962221211 -0.0,2.09218800047203 -1.91270082098484 -0.0,1.91270082098485 -2.09218800047202 -0.0,1.75238962221211 -2.29085116067364 -0.0,1.61028506886495 -2.50772096630085 -0.0,1.48537568064195 -2.74178593705221 -0.0,1.37664997724169 -2.99203459262631 -0.0,1.28309647836275 -3.25745545272172 -0.0,1.2037037037037 -3.53703703703703 -0.0,1.13746017296313 -3.82976786527082 -0.0,1.08335440583961 -4.13463645712166 -0.0,1.04037492203173 -4.45063133228814 -0.0,1.00751024123805 -4.77674101046882 -0.0,0.983748883157167 -5.11195401136229 -0.0,0.968079367487652 -5.45525885466714 -0.0,0.959490213928084 -5.80564406008193 -0.0,0.956969942177043 -6.16209814730525 -0.0,0.959507071933108 -6.52360963603567 -0.0,0.966090122894857 -6.88916704597178 -0.0,0.975707614760869 -7.25775889681216 -0.0,0.987348067229724 -7.62837370825537 -0.0,1 -8 -0.0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (8 -1 -0.0,7.62837370825536 -0.987348067229724 -0.0,7.25775889681215 -0.975707614760869 -0.0,6.88916704597178 -0.966090122894857 -0.0,6.52360963603567 -0.959507071933107 -0.0,6.16209814730525 -0.956969942177043 -0.0,5.80564406008193 -0.959490213928084 -0.0,5.45525885466714 -0.968079367487651 -0.0,5.11195401136229 -0.983748883157167 -0.0,4.77674101046882 -1.00751024123805 -0.0,4.45063133228814 -1.04037492203173 -0.0,4.13463645712167 -1.08335440583961 -0.0,3.82976786527082 -1.13746017296313 -0.0,3.53703703703704 -1.2037037037037 -0.0,3.25745545272173 -1.28309647836275 -0.0,2.99203459262631 -1.37664997724169 -0.0,2.74178593705221 -1.48537568064195 -0.0,2.50772096630085 -1.61028506886495 -0.0,2.29085116067365 -1.75238962221211 -0.0,2.09218800047203 -1.91270082098484 -0.0,1.91270082098485 -2.09218800047202 -0.0,1.75238962221211 -2.29085116067364 -0.0,1.61028506886495 -2.50772096630085 -0.0,1.48537568064195 -2.74178593705221 -0.0,1.37664997724169 -2.99203459262631 -0.0,1.28309647836275 -3.25745545272172 -0.0,1.2037037037037 -3.53703703703703 -0.0,1.13746017296313 -3.82976786527082 -0.0,1.08335440583961 -4.13463645712166 -0.0,1.04037492203173 -4.45063133228814 -0.0,1.00751024123805 -4.77674101046882 -0.0,0.983748883157167 -5.11195401136229 -0.0,0.968079367487652 -5.45525885466714 -0.0,0.959490213928084 -5.80564406008193 -0.0,0.956969942177043 -6.16209814730525 -0.0,0.959507071933108 -6.52360963603567 -0.0,0.966090122894857 -6.88916704597178 -0.0,0.975707614760869 -7.25775889681216 -0.0,0.987348067229724 -7.62837370825537 -0.0,1 -8 -0.0)",
    )

    # OGRFeature(entities):51
    #   EntityHandle (String) = 238
    #   POINT Z (7 -7 -7e-16)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (7 -7 -7e-16)")


###############################################################################
# OCS2WCS transformations 2. Also test RawCodeValues


def test_ogr_dxf_32():

    with gdal.config_option("DXF_INCLUDE_RAW_CODE_VALUES", "TRUE"):
        ds = ogr.Open("data/dxf/ocs2wcs2.dxf")
    lyr = ds.GetLayer(0)

    # INFO: Open of `ocs2wcs2.dxf' using driver `DXF' successful.

    # OGRFeature(entities):0
    #   EntityHandle (String) = 1B1
    #   POINT Z (4 4 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (4 4 0)")

    # OGRFeature(entities):1
    #   EntityHandle (String) = 1B2
    #   LINESTRING Z (0 0 0,1 1 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (0 0 0,1 1 0)")

    # OGRFeature(entities):2
    #   EntityHandle (String) = 1B3
    #   LINESTRING (1 1,2 1,1 2,1 1)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING (1 1,2 1,1 2,1 1)")
    assert feat.GetField("RawCodeValues") == ["330 1F", "43 0.0"]

    # OGRFeature(entities):3
    #   EntityHandle (String) = 1B4
    #   LINESTRING Z (1 1 0,1 2 0,2 2 0,1 1 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (1 1 0,1 2 0,2 2 0,1 1 0)")
    assert feat.GetField("RawCodeValues") == [
        "330 1F",
        "66      1",
        "10 0.0",
        "20 0.0",
        "30 0.0",
    ]

    # OGRFeature(entities):4
    #   EntityHandle (String) = 1B9
    #   LINESTRING Z (6 4 0,5.99512810051965 3.86048705251175 0,5.98053613748314 3.72165379807987 0,5.95629520146761 3.58417661836448 0,5.92252339187664 3.448725288366 0,5.87938524157182 3.31595971334866 0,5.8270909152852 3.1865267138484 0,5.76589518571785 3.06105687442822 0,5.69609619231285 2.94016147153359 0,5.61803398874989 2.82442949541505 0,5.53208888623796 2.71442478062692 0,5.4386796006773 2.61068325908201 0,5.33826121271772 2.51371034904521 0,5.23132295065132 2.42397849278656 0,5.11838580694149 2.34192485488992 0,5.0 2.26794919243112 0,4.87674229357815 2.20241190740167 0,4.74921318683182 2.14563229086643 0,4.61803398874989 2.09788696740969 0,4.48384379119934 2.05940854744801 0,4.34729635533386 2.03038449397558 0,4.20905692653531 2.01095620926345 0,4.069798993405 2.00121834596181 0,3.930201006595 2.00121834596181 0,3.79094307346469 2.01095620926345 0,3.65270364466614 2.03038449397558 0,3.51615620880066 2.05940854744801 0,3.38196601125011 2.09788696740969 0,3.25078681316818 2.14563229086643 0,3.12325770642185 2.20241190740167 0,3.0 2.26794919243112 0,2.88161419305851 2.34192485488992 0,2.76867704934868 2.42397849278656 0,2.66173878728228 2.51371034904521 0,2.5613203993227 2.61068325908201 0,2.46791111376204 2.71442478062692 0,2.38196601125011 2.82442949541505 0,2.30390380768715 2.94016147153359 0,2.23410481428215 3.06105687442822 0,2.1729090847148 3.1865267138484 0,2.12061475842818 3.31595971334866 0,2.07747660812336 3.448725288366 0,2.04370479853239 3.58417661836448 0,2.01946386251686 3.72165379807987 0,2.00487189948035 3.86048705251175 0,2.0 4.0 0,2.00487189948035 4.13951294748825 0,2.01946386251686 4.27834620192013 0,2.04370479853239 4.41582338163552 0,2.07747660812336 4.551274711634 0,2.12061475842818 4.68404028665134 0,2.1729090847148 4.8134732861516 0,2.23410481428215 4.93894312557178 0,2.30390380768715 5.05983852846641 0,2.38196601125011 5.17557050458495 0,2.46791111376204 5.28557521937308 0,2.5613203993227 5.38931674091799 0,2.66173878728228 5.48628965095479 0,2.76867704934868 5.57602150721344 0,2.88161419305851 5.65807514511008 0,3.0 5.73205080756888 0,3.12325770642184 5.79758809259833 0,3.25078681316818 5.85436770913357 0,3.38196601125011 5.90211303259031 0,3.51615620880066 5.94059145255199 0,3.65270364466614 5.96961550602442 0,3.79094307346469 5.98904379073655 0,3.930201006595 5.99878165403819 0,4.069798993405 5.99878165403819 0,4.20905692653531 5.98904379073655 0,4.34729635533386 5.96961550602442 0,4.48384379119933 5.94059145255199 0,4.61803398874989 5.90211303259031 0,4.74921318683182 5.85436770913357 0,4.87674229357815 5.79758809259833 0,5.0 5.73205080756888 0,5.11838580694149 5.65807514511008 0,5.23132295065132 5.57602150721344 0,5.33826121271772 5.48628965095479 0,5.4386796006773 5.389316740918 0,5.53208888623796 5.28557521937308 0,5.61803398874989 5.17557050458495 0,5.69609619231285 5.05983852846641 0,5.76589518571785 4.93894312557178 0,5.8270909152852 4.8134732861516 0,5.87938524157182 4.68404028665134 0,5.92252339187664 4.551274711634 0,5.95629520146761 4.41582338163552 0,5.98053613748314 4.27834620192013 0,5.99512810051965 4.13951294748825 0,6.0 4.0 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (6 4 0,5.99512810051965 3.86048705251175 0,5.98053613748314 3.72165379807987 0,5.95629520146761 3.58417661836448 0,5.92252339187664 3.448725288366 0,5.87938524157182 3.31595971334866 0,5.8270909152852 3.1865267138484 0,5.76589518571785 3.06105687442822 0,5.69609619231285 2.94016147153359 0,5.61803398874989 2.82442949541505 0,5.53208888623796 2.71442478062692 0,5.4386796006773 2.61068325908201 0,5.33826121271772 2.51371034904521 0,5.23132295065132 2.42397849278656 0,5.11838580694149 2.34192485488992 0,5.0 2.26794919243112 0,4.87674229357815 2.20241190740167 0,4.74921318683182 2.14563229086643 0,4.61803398874989 2.09788696740969 0,4.48384379119934 2.05940854744801 0,4.34729635533386 2.03038449397558 0,4.20905692653531 2.01095620926345 0,4.069798993405 2.00121834596181 0,3.930201006595 2.00121834596181 0,3.79094307346469 2.01095620926345 0,3.65270364466614 2.03038449397558 0,3.51615620880066 2.05940854744801 0,3.38196601125011 2.09788696740969 0,3.25078681316818 2.14563229086643 0,3.12325770642185 2.20241190740167 0,3.0 2.26794919243112 0,2.88161419305851 2.34192485488992 0,2.76867704934868 2.42397849278656 0,2.66173878728228 2.51371034904521 0,2.5613203993227 2.61068325908201 0,2.46791111376204 2.71442478062692 0,2.38196601125011 2.82442949541505 0,2.30390380768715 2.94016147153359 0,2.23410481428215 3.06105687442822 0,2.1729090847148 3.1865267138484 0,2.12061475842818 3.31595971334866 0,2.07747660812336 3.448725288366 0,2.04370479853239 3.58417661836448 0,2.01946386251686 3.72165379807987 0,2.00487189948035 3.86048705251175 0,2.0 4.0 0,2.00487189948035 4.13951294748825 0,2.01946386251686 4.27834620192013 0,2.04370479853239 4.41582338163552 0,2.07747660812336 4.551274711634 0,2.12061475842818 4.68404028665134 0,2.1729090847148 4.8134732861516 0,2.23410481428215 4.93894312557178 0,2.30390380768715 5.05983852846641 0,2.38196601125011 5.17557050458495 0,2.46791111376204 5.28557521937308 0,2.5613203993227 5.38931674091799 0,2.66173878728228 5.48628965095479 0,2.76867704934868 5.57602150721344 0,2.88161419305851 5.65807514511008 0,3.0 5.73205080756888 0,3.12325770642184 5.79758809259833 0,3.25078681316818 5.85436770913357 0,3.38196601125011 5.90211303259031 0,3.51615620880066 5.94059145255199 0,3.65270364466614 5.96961550602442 0,3.79094307346469 5.98904379073655 0,3.930201006595 5.99878165403819 0,4.069798993405 5.99878165403819 0,4.20905692653531 5.98904379073655 0,4.34729635533386 5.96961550602442 0,4.48384379119933 5.94059145255199 0,4.61803398874989 5.90211303259031 0,4.74921318683182 5.85436770913357 0,4.87674229357815 5.79758809259833 0,5.0 5.73205080756888 0,5.11838580694149 5.65807514511008 0,5.23132295065132 5.57602150721344 0,5.33826121271772 5.48628965095479 0,5.4386796006773 5.389316740918 0,5.53208888623796 5.28557521937308 0,5.61803398874989 5.17557050458495 0,5.69609619231285 5.05983852846641 0,5.76589518571785 4.93894312557178 0,5.8270909152852 4.8134732861516 0,5.87938524157182 4.68404028665134 0,5.92252339187664 4.551274711634 0,5.95629520146761 4.41582338163552 0,5.98053613748314 4.27834620192013 0,5.99512810051965 4.13951294748825 0,6.0 4.0 0)",
    )

    # OGRFeature(entities):5
    #   EntityHandle (String) = 1BA
    #   LINESTRING Z (2 4 0,4 4 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (2 4 0,4 4 0)")

    # OGRFeature(entities):6
    #   EntityHandle (String) = 1BB
    #   LINESTRING Z (4 4 0,6 4 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (4 4 0,6 4 0)")

    # OGRFeature(entities):7
    #   EntityHandle (String) = 1BC
    #   LINESTRING Z (4 3 0,4 4 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (4 3 0,4 4 0)")

    # OGRFeature(entities):8
    #   EntityHandle (String) = 1BD
    #   LINESTRING Z (4 4 0,4 5 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "LINESTRING Z (4 4 0,4 5 0)")

    # OGRFeature(entities):9
    #   EntityHandle (String) = 1BE
    #   LINESTRING Z (2 4 0,2.00487189948035 4.06975647374412 0,2.01946386251686 4.13917310096007 0,2.04370479853239 4.20791169081776 0,2.07747660812336 4.275637355817 0,2.12061475842818 4.34202014332567 0,2.1729090847148 4.4067366430758 0,2.23410481428215 4.46947156278589 0,2.30390380768715 4.52991926423321 0,2.38196601125011 4.58778525229247 0,2.46791111376204 4.64278760968654 0,2.5613203993227 4.694658370459 0,2.66173878728228 4.74314482547739 0,2.76867704934868 4.78801075360672 0,2.88161419305851 4.82903757255504 0,3.0 4.86602540378444 0,3.12325770642185 4.89879404629917 0,3.25078681316818 4.92718385456679 0,3.38196601125011 4.95105651629515 0,3.51615620880067 4.970295726276 0,3.65270364466614 4.98480775301221 0,3.79094307346469 4.99452189536827 0,3.930201006595 4.9993908270191 0,4.069798993405 4.9993908270191 0,4.20905692653531 4.99452189536827 0,4.34729635533386 4.98480775301221 0,4.48384379119934 4.970295726276 0,4.61803398874989 4.95105651629515 0,4.74921318683182 4.92718385456679 0,4.87674229357816 4.89879404629917 0,5.0 4.86602540378444 0,5.11838580694149 4.82903757255504 0,5.23132295065132 4.78801075360672 0,5.33826121271772 4.74314482547739 0,5.4386796006773 4.694658370459 0,5.53208888623796 4.64278760968654 0,5.61803398874989 4.58778525229247 0,5.69609619231285 4.5299192642332 0,5.76589518571785 4.46947156278589 0,5.8270909152852 4.4067366430758 0,5.87938524157182 4.34202014332567 0,5.92252339187664 4.275637355817 0,5.95629520146761 4.20791169081776 0,5.98053613748314 4.13917310096006 0,5.99512810051965 4.06975647374412 0,6.0 4.0 0,5.99512810051965 3.93024352625587 0,5.98053613748314 3.86082689903993 0,5.95629520146761 3.79208830918224 0,5.92252339187664 3.724362644183 0,5.87938524157182 3.65797985667433 0,5.8270909152852 3.5932633569242 0,5.76589518571785 3.53052843721411 0,5.69609619231285 3.4700807357668 0,5.61803398874989 3.41221474770753 0,5.53208888623796 3.35721239031346 0,5.4386796006773 3.305341629541 0,5.33826121271772 3.25685517452261 0,5.23132295065132 3.21198924639328 0,5.11838580694149 3.17096242744496 0,5.0 3.13397459621556 0,4.87674229357815 3.10120595370083 0,4.74921318683182 3.07281614543321 0,4.61803398874989 3.04894348370485 0,4.48384379119934 3.029704273724 0,4.34729635533386 3.01519224698779 0,4.20905692653531 3.00547810463173 0,4.069798993405 3.0006091729809 0,3.930201006595 3.0006091729809 0,3.79094307346469 3.00547810463173 0,3.65270364466614 3.01519224698779 0,3.51615620880066 3.029704273724 0,3.38196601125011 3.04894348370485 0,3.25078681316818 3.07281614543321 0,3.12325770642185 3.10120595370083 0,3.0 3.13397459621556 0,2.88161419305851 3.17096242744496 0,2.76867704934868 3.21198924639328 0,2.66173878728228 3.25685517452261 0,2.5613203993227 3.305341629541 0,2.46791111376204 3.35721239031346 0,2.38196601125011 3.41221474770753 0,2.30390380768715 3.4700807357668 0,2.23410481428215 3.53052843721411 0,2.1729090847148 3.5932633569242 0,2.12061475842818 3.65797985667433 0,2.07747660812336 3.724362644183 0,2.04370479853239 3.79208830918224 0,2.01946386251686 3.86082689903993 0,2.00487189948035 3.93024352625587 0,2 4 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 4 0,2.00487189948035 4.06975647374412 0,2.01946386251686 4.13917310096007 0,2.04370479853239 4.20791169081776 0,2.07747660812336 4.275637355817 0,2.12061475842818 4.34202014332567 0,2.1729090847148 4.4067366430758 0,2.23410481428215 4.46947156278589 0,2.30390380768715 4.52991926423321 0,2.38196601125011 4.58778525229247 0,2.46791111376204 4.64278760968654 0,2.5613203993227 4.694658370459 0,2.66173878728228 4.74314482547739 0,2.76867704934868 4.78801075360672 0,2.88161419305851 4.82903757255504 0,3.0 4.86602540378444 0,3.12325770642185 4.89879404629917 0,3.25078681316818 4.92718385456679 0,3.38196601125011 4.95105651629515 0,3.51615620880067 4.970295726276 0,3.65270364466614 4.98480775301221 0,3.79094307346469 4.99452189536827 0,3.930201006595 4.9993908270191 0,4.069798993405 4.9993908270191 0,4.20905692653531 4.99452189536827 0,4.34729635533386 4.98480775301221 0,4.48384379119934 4.970295726276 0,4.61803398874989 4.95105651629515 0,4.74921318683182 4.92718385456679 0,4.87674229357816 4.89879404629917 0,5.0 4.86602540378444 0,5.11838580694149 4.82903757255504 0,5.23132295065132 4.78801075360672 0,5.33826121271772 4.74314482547739 0,5.4386796006773 4.694658370459 0,5.53208888623796 4.64278760968654 0,5.61803398874989 4.58778525229247 0,5.69609619231285 4.5299192642332 0,5.76589518571785 4.46947156278589 0,5.8270909152852 4.4067366430758 0,5.87938524157182 4.34202014332567 0,5.92252339187664 4.275637355817 0,5.95629520146761 4.20791169081776 0,5.98053613748314 4.13917310096006 0,5.99512810051965 4.06975647374412 0,6.0 4.0 0,5.99512810051965 3.93024352625587 0,5.98053613748314 3.86082689903993 0,5.95629520146761 3.79208830918224 0,5.92252339187664 3.724362644183 0,5.87938524157182 3.65797985667433 0,5.8270909152852 3.5932633569242 0,5.76589518571785 3.53052843721411 0,5.69609619231285 3.4700807357668 0,5.61803398874989 3.41221474770753 0,5.53208888623796 3.35721239031346 0,5.4386796006773 3.305341629541 0,5.33826121271772 3.25685517452261 0,5.23132295065132 3.21198924639328 0,5.11838580694149 3.17096242744496 0,5.0 3.13397459621556 0,4.87674229357815 3.10120595370083 0,4.74921318683182 3.07281614543321 0,4.61803398874989 3.04894348370485 0,4.48384379119934 3.029704273724 0,4.34729635533386 3.01519224698779 0,4.20905692653531 3.00547810463173 0,4.069798993405 3.0006091729809 0,3.930201006595 3.0006091729809 0,3.79094307346469 3.00547810463173 0,3.65270364466614 3.01519224698779 0,3.51615620880066 3.029704273724 0,3.38196601125011 3.04894348370485 0,3.25078681316818 3.07281614543321 0,3.12325770642185 3.10120595370083 0,3.0 3.13397459621556 0,2.88161419305851 3.17096242744496 0,2.76867704934868 3.21198924639328 0,2.66173878728228 3.25685517452261 0,2.5613203993227 3.305341629541 0,2.46791111376204 3.35721239031346 0,2.38196601125011 3.41221474770753 0,2.30390380768715 3.4700807357668 0,2.23410481428215 3.53052843721411 0,2.1729090847148 3.5932633569242 0,2.12061475842818 3.65797985667433 0,2.07747660812336 3.724362644183 0,2.04370479853239 3.79208830918224 0,2.01946386251686 3.86082689903993 0,2.00487189948035 3.93024352625587 0,2 4 0)",
    )

    # OGRFeature(entities):10
    #   EntityHandle (String) = 1BF
    #   LINESTRING Z (2.0 2.0 0,1.96657794502105 2.03582232791524 0,1.93571660708646 2.07387296203834 0,1.90756413746468 2.11396923855471 0,1.88225568337755 2.15591867344963 0,1.85991273921989 2.19951988653655 0,1.84064256332004 2.24456356819194 0,1.8245376630414 2.29083348415575 0,1.81167535069652 2.33810751357387 0,1.80211737240583 2.38615871529951 0,1.79590961168258 2.43475641733454 0,1.79308186916688 2.48366732418105 0,1.79364771956639 2.53265663678705 0,1.79760444649032 2.58148917971011 0,1.80493305548955 2.62993053008785 0,1.81559836524041 2.67774814299566 0,1.8295491764342 2.72471246778926 0,1.84671851756181 2.7705980500731 0,1.86702396641357 2.81518461400453 0,1.89036804575079 2.85825811973811 0,1.91663869124976 2.89961179093366 0,1.94570978947168 2.93904710739563 0,1.97744178327594 2.97637475807832 0,2.01168234177068 3.01141554988232 0,2.04826709158413 3.04400126787917 0,2.08702040594658 3.07397548283483 0,2.12775624779472 3.10119430215541 0,2.17027906285109 3.12552706065018 0,2.2143847183914 3.14685694779575 0,2.25986148319297 3.16508156849045 0,2.30649104396024 3.18011343460661 0,2.35404955334774 3.1918803849814 0,2.40230870454951 3.20032593182975 0,2.45103682729644 3.2054095319166 0,2.5 3.20710678118655 0,2.54896317270356 3.2054095319166 0,2.59769129545049 3.20032593182975 0,2.64595044665226 3.1918803849814 0,2.69350895603976 3.18011343460661 0,2.74013851680703 3.16508156849045 0,2.7856152816086 3.14685694779575 0,2.8297209371489 3.12552706065018 0,2.87224375220528 3.10119430215541 0,2.91297959405342 3.07397548283483 0,2.95173290841587 3.04400126787917 0,2.98831765822932 3.01141554988232 0,3.02255821672406 2.97637475807832 0,3.05429021052832 2.93904710739563 0,3.08336130875024 2.89961179093367 0,3.10963195424921 2.85825811973811 0,3.13297603358643 2.81518461400453 0,3.15328148243819 2.7705980500731 0,3.1704508235658 2.72471246778926 0,3.18440163475959 2.67774814299567 0,3.19506694451045 2.62993053008786 0,3.20239555350968 2.58148917971011 0,3.20635228043361 2.53265663678705 0,3.20691813083312 2.48366732418105 0,3.20409038831742 2.43475641733454 0,3.19788262759417 2.38615871529951 0,3.18832464930348 2.33810751357387 0,3.1754623369586 2.29083348415575 0,3.15935743667996 2.24456356819194 0,3.14008726078011 2.19951988653655 0,3.11774431662245 2.15591867344963 0,3.09243586253532 2.11396923855472 0,3.06428339291354 2.07387296203834 0,3.03342205497895 2.03582232791524 0,3 2 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2.0 2.0 0,1.96657794502105 2.03582232791524 0,1.93571660708646 2.07387296203834 0,1.90756413746468 2.11396923855471 0,1.88225568337755 2.15591867344963 0,1.85991273921989 2.19951988653655 0,1.84064256332004 2.24456356819194 0,1.8245376630414 2.29083348415575 0,1.81167535069652 2.33810751357387 0,1.80211737240583 2.38615871529951 0,1.79590961168258 2.43475641733454 0,1.79308186916688 2.48366732418105 0,1.79364771956639 2.53265663678705 0,1.79760444649032 2.58148917971011 0,1.80493305548955 2.62993053008785 0,1.81559836524041 2.67774814299566 0,1.8295491764342 2.72471246778926 0,1.84671851756181 2.7705980500731 0,1.86702396641357 2.81518461400453 0,1.89036804575079 2.85825811973811 0,1.91663869124976 2.89961179093366 0,1.94570978947168 2.93904710739563 0,1.97744178327594 2.97637475807832 0,2.01168234177068 3.01141554988232 0,2.04826709158413 3.04400126787917 0,2.08702040594658 3.07397548283483 0,2.12775624779472 3.10119430215541 0,2.17027906285109 3.12552706065018 0,2.2143847183914 3.14685694779575 0,2.25986148319297 3.16508156849045 0,2.30649104396024 3.18011343460661 0,2.35404955334774 3.1918803849814 0,2.40230870454951 3.20032593182975 0,2.45103682729644 3.2054095319166 0,2.5 3.20710678118655 0,2.54896317270356 3.2054095319166 0,2.59769129545049 3.20032593182975 0,2.64595044665226 3.1918803849814 0,2.69350895603976 3.18011343460661 0,2.74013851680703 3.16508156849045 0,2.7856152816086 3.14685694779575 0,2.8297209371489 3.12552706065018 0,2.87224375220528 3.10119430215541 0,2.91297959405342 3.07397548283483 0,2.95173290841587 3.04400126787917 0,2.98831765822932 3.01141554988232 0,3.02255821672406 2.97637475807832 0,3.05429021052832 2.93904710739563 0,3.08336130875024 2.89961179093367 0,3.10963195424921 2.85825811973811 0,3.13297603358643 2.81518461400453 0,3.15328148243819 2.7705980500731 0,3.1704508235658 2.72471246778926 0,3.18440163475959 2.67774814299567 0,3.19506694451045 2.62993053008786 0,3.20239555350968 2.58148917971011 0,3.20635228043361 2.53265663678705 0,3.20691813083312 2.48366732418105 0,3.20409038831742 2.43475641733454 0,3.19788262759417 2.38615871529951 0,3.18832464930348 2.33810751357387 0,3.1754623369586 2.29083348415575 0,3.15935743667996 2.24456356819194 0,3.14008726078011 2.19951988653655 0,3.11774431662245 2.15591867344963 0,3.09243586253532 2.11396923855472 0,3.06428339291354 2.07387296203834 0,3.03342205497895 2.03582232791524 0,3 2 0)",
    )

    # OGRFeature(entities):11
    #   EntityHandle (String) = 1C0
    #   POLYGON Z ((1 2 0,1 3 0,2 3 0,2 2 0,1 2 0))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POLYGON Z ((1 2 0,1 3 0,2 3 0,2 2 0,1 2 0))")

    # OGRFeature(entities):12
    #   EntityHandle (String) = 1C1
    #   POLYGON ((3 4,4 4,4 3,3 3,3 4))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POLYGON ((3 4,4 4,4 3,3 3,3 4))")

    # OGRFeature(entities):13
    #   EntityHandle (String) = 1C3
    #   POLYGON Z ((8 8 0,9 8 0,9 9 0,8 9 0,8 8 0))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POLYGON Z ((8 8 0,9 8 0,9 9 0,8 9 0,8 8 0))")

    # OGRFeature(entities):14
    #   EntityHandle (String) = 1C6
    #   LINESTRING Z (2 2 0,2.15384615384615 2.15384615384615 0,2.30769230769231 2.30769230769231 0,2.46153846153846 2.46153846153846 0,2.61538461538461 2.61538461538461 0,2.76923076923077 2.76923076923077 0,2.92307692307692 2.92307692307692 0,3.07692307692308 3.07692307692308 0,3.23076923076923 3.23076923076923 0,3.38461538461538 3.38461538461538 0,3.53846153846154 3.53846153846154 0,3.69230769230769 3.69230769230769 0,3.84615384615385 3.84615384615385 0,4 4 0,4.15384615384615 4.15384615384615 0,4.30769230769231 4.30769230769231 0,4.46153846153846 4.46153846153846 0,4.61538461538462 4.61538461538462 0,4.76923076923077 4.76923076923077 0,4.92307692307692 4.92307692307692 0,5.07692307692308 5.07692307692308 0,5.23076923076923 5.23076923076923 0,5.38461538461538 5.38461538461538 0,5.53846153846154 5.53846153846154 0,5.69230769230769 5.69230769230769 0,5.84615384615385 5.84615384615385 0,6.0 6.0 0,6.15384615384615 6.15384615384615 0,6.30769230769231 6.30769230769231 0,6.46153846153846 6.46153846153846 0,6.61538461538462 6.61538461538462 0,6.76923076923077 6.76923076923077 0,6.92307692307692 6.92307692307692 0,7.07692307692308 7.07692307692308 0,7.23076923076923 7.23076923076923 0,7.38461538461539 7.38461538461539 0,7.53846153846154 7.53846153846154 0,7.69230769230769 7.69230769230769 0,7.84615384615385 7.84615384615385 0,8 8 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2 2 0,2.15384615384615 2.15384615384615 0,2.30769230769231 2.30769230769231 0,2.46153846153846 2.46153846153846 0,2.61538461538461 2.61538461538461 0,2.76923076923077 2.76923076923077 0,2.92307692307692 2.92307692307692 0,3.07692307692308 3.07692307692308 0,3.23076923076923 3.23076923076923 0,3.38461538461538 3.38461538461538 0,3.53846153846154 3.53846153846154 0,3.69230769230769 3.69230769230769 0,3.84615384615385 3.84615384615385 0,4 4 0,4.15384615384615 4.15384615384615 0,4.30769230769231 4.30769230769231 0,4.46153846153846 4.46153846153846 0,4.61538461538462 4.61538461538462 0,4.76923076923077 4.76923076923077 0,4.92307692307692 4.92307692307692 0,5.07692307692308 5.07692307692308 0,5.23076923076923 5.23076923076923 0,5.38461538461538 5.38461538461538 0,5.53846153846154 5.53846153846154 0,5.69230769230769 5.69230769230769 0,5.84615384615385 5.84615384615385 0,6.0 6.0 0,6.15384615384615 6.15384615384615 0,6.30769230769231 6.30769230769231 0,6.46153846153846 6.46153846153846 0,6.61538461538462 6.61538461538462 0,6.76923076923077 6.76923076923077 0,6.92307692307692 6.92307692307692 0,7.07692307692308 7.07692307692308 0,7.23076923076923 7.23076923076923 0,7.38461538461539 7.38461538461539 0,7.53846153846154 7.53846153846154 0,7.69230769230769 7.69230769230769 0,7.84615384615385 7.84615384615385 0,8 8 0)",
    )

    # OGRFeature(entities):15
    #   EntityHandle (String) = 1C7
    #   LINESTRING Z (8 1 0,7.62837370825536 0.987348067229724 0,7.25775889681215 0.975707614760869 0,6.88916704597178 0.966090122894857 0,6.52360963603567 0.959507071933107 0,6.16209814730525 0.956969942177043 0,5.80564406008193 0.959490213928084 0,5.45525885466714 0.968079367487651 0,5.11195401136229 0.983748883157167 0,4.77674101046882 1.00751024123805 0,4.45063133228814 1.04037492203173 0,4.13463645712167 1.08335440583961 0,3.82976786527082 1.13746017296313 0,3.53703703703704 1.2037037037037 0,3.25745545272173 1.28309647836275 0,2.99203459262631 1.37664997724169 0,2.74178593705221 1.48537568064195 0,2.50772096630085 1.61028506886495 0,2.29085116067365 1.75238962221211 0,2.09218800047203 1.91270082098484 0,1.91270082098485 2.09218800047202 0,1.75238962221211 2.29085116067364 0,1.61028506886495 2.50772096630085 0,1.48537568064195 2.74178593705221 0,1.37664997724169 2.99203459262631 0,1.28309647836275 3.25745545272172 0,1.2037037037037 3.53703703703703 0,1.13746017296313 3.82976786527082 0,1.08335440583961 4.13463645712166 0,1.04037492203173 4.45063133228814 0,1.00751024123805 4.77674101046882 0,0.983748883157167 5.11195401136229 0,0.968079367487652 5.45525885466714 0,0.959490213928084 5.80564406008193 0,0.956969942177043 6.16209814730525 0,0.959507071933108 6.52360963603567 0,0.966090122894857 6.88916704597178 0,0.975707614760869 7.25775889681216 0,0.987348067229724 7.62837370825537 0,1 8 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (8 1 0,7.62837370825536 0.987348067229724 0,7.25775889681215 0.975707614760869 0,6.88916704597178 0.966090122894857 0,6.52360963603567 0.959507071933107 0,6.16209814730525 0.956969942177043 0,5.80564406008193 0.959490213928084 0,5.45525885466714 0.968079367487651 0,5.11195401136229 0.983748883157167 0,4.77674101046882 1.00751024123805 0,4.45063133228814 1.04037492203173 0,4.13463645712167 1.08335440583961 0,3.82976786527082 1.13746017296313 0,3.53703703703704 1.2037037037037 0,3.25745545272173 1.28309647836275 0,2.99203459262631 1.37664997724169 0,2.74178593705221 1.48537568064195 0,2.50772096630085 1.61028506886495 0,2.29085116067365 1.75238962221211 0,2.09218800047203 1.91270082098484 0,1.91270082098485 2.09218800047202 0,1.75238962221211 2.29085116067364 0,1.61028506886495 2.50772096630085 0,1.48537568064195 2.74178593705221 0,1.37664997724169 2.99203459262631 0,1.28309647836275 3.25745545272172 0,1.2037037037037 3.53703703703703 0,1.13746017296313 3.82976786527082 0,1.08335440583961 4.13463645712166 0,1.04037492203173 4.45063133228814 0,1.00751024123805 4.77674101046882 0,0.983748883157167 5.11195401136229 0,0.968079367487652 5.45525885466714 0,0.959490213928084 5.80564406008193 0,0.956969942177043 6.16209814730525 0,0.959507071933108 6.52360963603567 0,0.966090122894857 6.88916704597178 0,0.975707614760869 7.25775889681216 0,0.987348067229724 7.62837370825537 0,1 8 0)",
    )

    # OGRFeature(entities):16
    #   EntityHandle (String) = 1C8
    #   POINT Z (7 7 0)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (7 7 0)")

    # OGRFeature(entities):17
    #   EntityHandle (String) = 1C9
    #   POINT Z (-2.0 4.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (-2.0 4.0 -3.46410161513775)")

    # OGRFeature(entities):18
    #   EntityHandle (String) = 1CA
    #   LINESTRING Z (0 0 0,-0.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (0 0 0,-0.5 1.0 -0.866025403784439)"
    )

    # OGRFeature(entities):19
    #   EntityHandle (String) = 1CB
    #   LINESTRING Z (-0.5 1.0 -0.866025403784439,-1.0 1.0 -1.73205080756888,-0.5 2.0 -0.866025403784439,-0.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-0.5 1.0 -0.866025403784439,-1.0 1.0 -1.73205080756888,-0.5 2.0 -0.866025403784439,-0.5 1.0 -0.866025403784439)",
    )

    # OGRFeature(entities):20
    #   EntityHandle (String) = 1CC
    #   LINESTRING Z (-0.5 1.0 -0.866025403784439,-0.5 2.0 -0.866025403784439,-1.0 2.0 -1.73205080756888,-0.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-0.5 1.0 -0.866025403784439,-0.5 2.0 -0.866025403784439,-1.0 2.0 -1.73205080756888,-0.5 1.0 -0.866025403784439)",
    )

    # OGRFeature(entities):21
    #   EntityHandle (String) = 1D1
    #   LINESTRING Z (-2.0 6.0 -3.46410161513776,-2.06975647374412 5.99512810051965 -3.58492337181942,-2.13917310096006 5.98053613748314 -3.7051564970475,-2.20791169081776 5.95629520146761 -3.82421522712167,-2.275637355817 5.92252339187664 -3.94151951987674,-2.34202014332567 5.87938524157182 -4.0564978805898,-2.4067366430758 5.8270909152852 -4.16859014624505,-2.46947156278589 5.76589518571785 -4.27725021459168,-2.5299192642332 5.69609619231285 -4.38194870469918,-2.58778525229247 5.61803398874989 -4.48217553604801,-2.64278760968654 5.53208888623796 -4.57744241359059,-2.694658370459 5.4386796006773 -4.66728520667574,-2.74314482547739 5.33826121271772 -4.75126621024651,-2.78801075360672 5.23132295065132 -4.82897627729524,-2.82903757255504 5.11838580694149 -4.90003681218666,-2.86602540378444 5.0 -4.96410161513776,-2.89879404629917 4.87674229357815 -5.02085856886833,-2.92718385456679 4.74921318683182 -5.07003115920498,-2.95105651629515 4.61803398874989 -5.11137982223042,-2.970295726276 4.48384379119934 -5.14470311141473,-2.98480775301221 4.34729635533386 -5.16983867904264,-2.99452189536827 4.20905692653531 -5.1866640671553,-2.99939082701909 4.069798993405 -5.19509730415311,-2.99939082701909 3.930201006595 -5.19509730415311,-2.99452189536827 3.79094307346469 -5.1866640671553,-2.98480775301221 3.65270364466614 -5.16983867904264,-2.970295726276 3.51615620880066 -5.14470311141473,-2.95105651629515 3.38196601125011 -5.11137982223042,-2.92718385456679 3.25078681316818 -5.07003115920498,-2.89879404629917 3.12325770642185 -5.02085856886833,-2.86602540378444 3.0 -4.96410161513776,-2.82903757255504 2.88161419305851 -4.90003681218666,-2.78801075360672 2.76867704934868 -4.82897627729524,-2.74314482547739 2.66173878728228 -4.75126621024651,-2.694658370459 2.5613203993227 -4.66728520667574,-2.64278760968654 2.46791111376204 -4.5774424135906,-2.58778525229247 2.38196601125011 -4.48217553604801,-2.5299192642332 2.30390380768715 -4.38194870469918,-2.46947156278589 2.23410481428215 -4.27725021459168,-2.4067366430758 2.1729090847148 -4.16859014624505,-2.34202014332567 2.12061475842818 -4.0564978805898,-2.275637355817 2.07747660812336 -3.94151951987674,-2.20791169081776 2.04370479853239 -3.82421522712167,-2.13917310096006 2.01946386251686 -3.7051564970475,-2.06975647374412 2.00487189948035 -3.58492337181943,-2.0 2.0 -3.46410161513776,-1.93024352625587 2.00487189948035 -3.34327985845609,-1.86082689903993 2.01946386251686 -3.22304673322801,-1.79208830918224 2.04370479853239 -3.10398800315384,-1.724362644183 2.07747660812336 -2.98668371039877,-1.65797985667433 2.12061475842818 -2.87170534968571,-1.5932633569242 2.1729090847148 -2.75961308403046,-1.53052843721411 2.23410481428215 -2.65095301568383,
    # -1.47008073576679 2.30390380768715 -2.54625452557633,-1.41221474770753 2.38196601125011 -2.4460276942275,-1.35721239031346 2.46791111376204 -2.35076081668492,-1.305341629541 2.5613203993227 -2.26091802359977,-1.25685517452261 2.66173878728228 -2.176937020029,-1.21198924639328 2.76867704934868 -2.09922695298027,-1.17096242744496 2.88161419305851 -2.02816641808885,-1.13397459621556 3.0 -1.96410161513776,-1.10120595370083 3.12325770642184 -1.90734466140718,-1.07281614543321 3.25078681316818 -1.85817207107053,-1.04894348370485 3.38196601125011 -1.81682340804509,-1.029704273724 3.51615620880066 -1.78350011886079,-1.01519224698779 3.65270364466614 -1.75836455123287,-1.00547810463173 3.79094307346469 -1.74153916312021,-1.0006091729809 3.930201006595 -1.7331059261224,-1.0006091729809 4.069798993405 -1.7331059261224,-1.00547810463173 4.20905692653531 -1.74153916312021,-1.01519224698779 4.34729635533386 -1.75836455123287,-1.029704273724 4.48384379119933 -1.78350011886078,-1.04894348370485 4.61803398874989 -1.81682340804509,-1.07281614543321 4.74921318683182 -1.85817207107053,-1.10120595370083 4.87674229357815 -1.90734466140718,-1.13397459621556 5.0 -1.96410161513776,-1.17096242744496 5.11838580694149 -2.02816641808885,-1.21198924639328 5.23132295065132 -2.09922695298027,-1.25685517452261 5.33826121271772 -2.176937020029,-1.305341629541 5.4386796006773 -2.26091802359977,-1.35721239031346 5.53208888623796 -2.35076081668492,-1.41221474770753 5.61803398874989 -2.4460276942275,-1.47008073576679 5.69609619231285 -2.54625452557633,-1.53052843721411 5.76589518571785 -2.65095301568383,-1.5932633569242 5.8270909152852 -2.75961308403046,-1.65797985667433 5.87938524157182 -2.87170534968571,-1.724362644183 5.92252339187664 -2.98668371039877,-1.79208830918224 5.95629520146761 -3.10398800315384,-1.86082689903993 5.98053613748314 -3.22304673322801,-1.93024352625587 5.99512810051965 -3.34327985845609,-2.0 6.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-2.0 6.0 -3.46410161513776,-2.06975647374412 5.99512810051965 -3.58492337181942,-2.13917310096006 5.98053613748314 -3.7051564970475,-2.20791169081776 5.95629520146761 -3.82421522712167,-2.275637355817 5.92252339187664 -3.94151951987674,-2.34202014332567 5.87938524157182 -4.0564978805898,-2.4067366430758 5.8270909152852 -4.16859014624505,-2.46947156278589 5.76589518571785 -4.27725021459168,-2.5299192642332 5.69609619231285 -4.38194870469918,-2.58778525229247 5.61803398874989 -4.48217553604801,-2.64278760968654 5.53208888623796 -4.57744241359059,-2.694658370459 5.4386796006773 -4.66728520667574,-2.74314482547739 5.33826121271772 -4.75126621024651,-2.78801075360672 5.23132295065132 -4.82897627729524,-2.82903757255504 5.11838580694149 -4.90003681218666,-2.86602540378444 5.0 -4.96410161513776,-2.89879404629917 4.87674229357815 -5.02085856886833,-2.92718385456679 4.74921318683182 -5.07003115920498,-2.95105651629515 4.61803398874989 -5.11137982223042,-2.970295726276 4.48384379119934 -5.14470311141473,-2.98480775301221 4.34729635533386 -5.16983867904264,-2.99452189536827 4.20905692653531 -5.1866640671553,-2.99939082701909 4.069798993405 -5.19509730415311,-2.99939082701909 3.930201006595 -5.19509730415311,-2.99452189536827 3.79094307346469 -5.1866640671553,-2.98480775301221 3.65270364466614 -5.16983867904264,-2.970295726276 3.51615620880066 -5.14470311141473,-2.95105651629515 3.38196601125011 -5.11137982223042,-2.92718385456679 3.25078681316818 -5.07003115920498,-2.89879404629917 3.12325770642185 -5.02085856886833,-2.86602540378444 3.0 -4.96410161513776,-2.82903757255504 2.88161419305851 -4.90003681218666,-2.78801075360672 2.76867704934868 -4.82897627729524,-2.74314482547739 2.66173878728228 -4.75126621024651,-2.694658370459 2.5613203993227 -4.66728520667574,-2.64278760968654 2.46791111376204 -4.5774424135906,-2.58778525229247 2.38196601125011 -4.48217553604801,-2.5299192642332 2.30390380768715 -4.38194870469918,-2.46947156278589 2.23410481428215 -4.27725021459168,-2.4067366430758 2.1729090847148 -4.16859014624505,-2.34202014332567 2.12061475842818 -4.0564978805898,-2.275637355817 2.07747660812336 -3.94151951987674,-2.20791169081776 2.04370479853239 -3.82421522712167,-2.13917310096006 2.01946386251686 -3.7051564970475,-2.06975647374412 2.00487189948035 -3.58492337181943,-2.0 2.0 -3.46410161513776,-1.93024352625587 2.00487189948035 -3.34327985845609,-1.86082689903993 2.01946386251686 -3.22304673322801,-1.79208830918224 2.04370479853239 -3.10398800315384,-1.724362644183 2.07747660812336 -2.98668371039877,"
        + "-1.65797985667433 2.12061475842818 -2.87170534968571,-1.5932633569242 2.1729090847148 -2.75961308403046,-1.53052843721411 2.23410481428215 -2.65095301568383,-1.47008073576679 2.30390380768715 -2.54625452557633,-1.41221474770753 2.38196601125011 -2.4460276942275,-1.35721239031346 2.46791111376204 -2.35076081668492,-1.305341629541 2.5613203993227 -2.26091802359977,-1.25685517452261 2.66173878728228 -2.176937020029,-1.21198924639328 2.76867704934868 -2.09922695298027,-1.17096242744496 2.88161419305851 -2.02816641808885,-1.13397459621556 3.0 -1.96410161513776,-1.10120595370083 3.12325770642184 -1.90734466140718,-1.07281614543321 3.25078681316818 -1.85817207107053,-1.04894348370485 3.38196601125011 -1.81682340804509,-1.029704273724 3.51615620880066 -1.78350011886079,-1.01519224698779 3.65270364466614 -1.75836455123287,-1.00547810463173 3.79094307346469 -1.74153916312021,-1.0006091729809 3.930201006595 -1.7331059261224,-1.0006091729809 4.069798993405 -1.7331059261224,-1.00547810463173 4.20905692653531 -1.74153916312021,-1.01519224698779 4.34729635533386 -1.75836455123287,-1.029704273724 4.48384379119933 -1.78350011886078,-1.04894348370485 4.61803398874989 -1.81682340804509,-1.07281614543321 4.74921318683182 -1.85817207107053,-1.10120595370083 4.87674229357815 -1.90734466140718,-1.13397459621556 5.0 -1.96410161513776,-1.17096242744496 5.11838580694149 -2.02816641808885,-1.21198924639328 5.23132295065132 -2.09922695298027,-1.25685517452261 5.33826121271772 -2.176937020029,-1.305341629541 5.4386796006773 -2.26091802359977,-1.35721239031346 5.53208888623796 -2.35076081668492,-1.41221474770753 5.61803398874989 -2.4460276942275,-1.47008073576679 5.69609619231285 -2.54625452557633,-1.53052843721411 5.76589518571785 -2.65095301568383,-1.5932633569242 5.8270909152852 -2.75961308403046,-1.65797985667433 5.87938524157182 -2.87170534968571,-1.724362644183 5.92252339187664 -2.98668371039877,-1.79208830918224 5.95629520146761 -3.10398800315384,-1.86082689903993 5.98053613748314 -3.22304673322801,-1.93024352625587 5.99512810051965 -3.34327985845609,-2.0 6.0 -3.46410161513775)",
    )

    # OGRFeature(entities):22
    #   EntityHandle (String) = 1D2
    #   LINESTRING Z (-1.0 4.0 -1.73205080756888,-2.0 4.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (-1.0 4.0 -1.73205080756888,-2.0 4.0 -3.46410161513775)"
    )

    # OGRFeature(entities):23
    #   EntityHandle (String) = 1D3
    #   LINESTRING Z (-2.0 4.0 -3.46410161513775,-3.0 4.0 -5.19615242270663)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (-2.0 4.0 -3.46410161513775,-3.0 4.0 -5.19615242270663)"
    )

    # OGRFeature(entities):24
    #   EntityHandle (String) = 1D4
    #   LINESTRING Z (-2.0 3.0 -3.46410161513775,-2.0 4.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (-2.0 3.0 -3.46410161513775,-2.0 4.0 -3.46410161513775)"
    )

    # OGRFeature(entities):25
    #   EntityHandle (String) = 1D5
    #   LINESTRING Z (-2.0 4.0 -3.46410161513775,-2.0 5.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (-2.0 4.0 -3.46410161513775,-2.0 5.0 -3.46410161513775)"
    )

    # OGRFeature(entities):26
    #   EntityHandle (String) = 1D6
    #   LINESTRING Z (-1.0 4.0 -1.73205080756888,-1.00243594974018 4.06975647374412 -1.73626999628355,-1.00973193125843 4.13917310096007 -1.74890700696425,-1.02185239926619 4.20791169081776 -1.76990027336521,-1.03873830406168 4.275637355817 -1.79914751840276,-1.06030737921409 4.34202014332567 -1.83650625243901,-1.0864545423574 4.4067366430758 -1.88179446747701,-1.11705240714107 4.46947156278589 -1.93479152388545,-1.15195190384357 4.52991926423321 -1.99523922533277,-1.19098300562505 4.58778525229247 -2.06284307669368,-1.23395555688102 4.64278760968654 -2.13727371879988,-1.28066019966135 4.694658370459 -2.21816853304476,-1.33086939364114 4.74314482547739 -2.30513340802484,-1.38433852467434 4.78801075360672 -2.3977446596109,-1.44080709652925 4.82903757255504 -2.49555109509446,-1.5 4.86602540378444 -2.59807621135332,-1.56162885321092 4.89879404629917 -2.70482051632684,-1.62539340658409 4.92718385456679 -2.8152639624911,-1.69098300562505 4.95105651629515 -2.92886848047812,-1.75807810440033 4.970295726276 -3.04508060049576,-1.82635182233307 4.98480775301221 -3.16333414877688,-1.89547153673235 4.99452189536827 -3.28305300592108,-1.9651005032975 4.99939082701909 -3.40365391369044,-2.0348994967025 4.99939082701909 -3.52454931658507,-2.10452846326765 4.99452189536827 -3.64515022435443,-2.17364817766693 4.98480775301221 -3.76486908149863,-2.24192189559967 4.970295726276 -3.88312262977975,-2.30901699437495 4.95105651629515 -3.99933474979739,-2.37460659341591 4.92718385456679 -4.11293926778441,-2.43837114678908 4.89879404629917 -4.22338271394867,-2.5 4.86602540378444 -4.33012701892219,-2.55919290347075 4.82903757255504 -4.43265213518105,-2.61566147532566 4.78801075360672 -4.53045857066461,-2.66913060635886 4.74314482547739 -4.62306982225067,-2.71933980033865 4.694658370459 -4.71003469723075,-2.76604444311898 4.64278760968654 -4.79092951147563,-2.80901699437495 4.58778525229247 -4.86536015358183,-2.84804809615642 4.5299192642332 -4.93296400494274,-2.88294759285893 4.46947156278589 -4.99341170639005,-2.9135454576426 4.4067366430758 -5.0464087627985,-2.93969262078591 4.34202014332567 -5.0916969778365,-2.96126169593832 4.275637355817 -5.12905571187275,-2.9781476007338 4.20791169081776 -5.1583029569103,-2.99026806874157 4.13917310096007 -5.17929622331126,-2.99756405025982 4.06975647374412 -5.19193323399196,-3.0 4.0 -5.19615242270663,-2.99756405025982 3.93024352625587 -5.19193323399196,-2.99026806874157 3.86082689903993 -5.17929622331126,-2.9781476007338 3.79208830918224 -5.1583029569103,-2.96126169593832 3.724362644183 -5.12905571187275,
    # -2.93969262078591 3.65797985667433 -5.0916969778365,-2.9135454576426 3.5932633569242 -5.0464087627985,-2.88294759285893 3.53052843721411 -4.99341170639005,-2.84804809615642 3.4700807357668 -4.93296400494274,-2.80901699437495 3.41221474770753 -4.86536015358183,-2.76604444311898 3.35721239031346 -4.79092951147563,-2.71933980033865 3.305341629541 -4.71003469723075,-2.66913060635886 3.25685517452261 -4.62306982225067,-2.61566147532566 3.21198924639328 -4.53045857066461,-2.55919290347075 3.17096242744496 -4.43265213518105,-2.5 3.13397459621556 -4.33012701892219,-2.43837114678908 3.10120595370083 -4.22338271394867,-2.37460659341591 3.07281614543321 -4.11293926778441,-2.30901699437495 3.04894348370485 -3.99933474979739,-2.24192189559967 3.029704273724 -3.88312262977975,-2.17364817766693 3.01519224698779 -3.76486908149863,-2.10452846326765 3.00547810463173 -3.64515022435443,-2.0348994967025 3.0006091729809 -3.52454931658507,-1.9651005032975 3.0006091729809 -3.40365391369044,-1.89547153673235 3.00547810463173 -3.28305300592108,-1.82635182233307 3.01519224698779 -3.16333414877688,-1.75807810440033 3.029704273724 -3.04508060049576,-1.69098300562505 3.04894348370485 -2.92886848047812,-1.62539340658409 3.07281614543321 -2.8152639624911,-1.56162885321092 3.10120595370083 -2.70482051632684,-1.5 3.13397459621556 -2.59807621135332,-1.44080709652925 3.17096242744496 -2.49555109509446,-1.38433852467434 3.21198924639328 -2.3977446596109,-1.33086939364114 3.25685517452261 -2.30513340802484,-1.28066019966135 3.305341629541 -2.21816853304476,-1.23395555688102 3.35721239031346 -2.13727371879988,-1.19098300562505 3.41221474770753 -2.06284307669368,-1.15195190384357 3.4700807357668 -1.99523922533277,-1.11705240714107 3.53052843721411 -1.93479152388545,-1.0864545423574 3.5932633569242 -1.88179446747701,-1.06030737921409 3.65797985667433 -1.83650625243901,-1.03873830406168 3.724362644183 -1.79914751840276,-1.02185239926619 3.79208830918224 -1.76990027336521,-1.00973193125843 3.86082689903993 -1.74890700696425,-1.00243594974018 3.93024352625587 -1.73626999628355,-1.0 4.0 -1.73205080756888)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-1.0 4.0 -1.73205080756888,-1.00243594974018 4.06975647374412 -1.73626999628355,-1.00973193125843 4.13917310096007 -1.74890700696425,-1.02185239926619 4.20791169081776 -1.76990027336521,-1.03873830406168 4.275637355817 -1.79914751840276,-1.06030737921409 4.34202014332567 -1.83650625243901,-1.0864545423574 4.4067366430758 -1.88179446747701,-1.11705240714107 4.46947156278589 -1.93479152388545,-1.15195190384357 4.52991926423321 -1.99523922533277,-1.19098300562505 4.58778525229247 -2.06284307669368,-1.23395555688102 4.64278760968654 -2.13727371879988,-1.28066019966135 4.694658370459 -2.21816853304476,-1.33086939364114 4.74314482547739 -2.30513340802484,-1.38433852467434 4.78801075360672 -2.3977446596109,-1.44080709652925 4.82903757255504 -2.49555109509446,-1.5 4.86602540378444 -2.59807621135332,-1.56162885321092 4.89879404629917 -2.70482051632684,-1.62539340658409 4.92718385456679 -2.8152639624911,-1.69098300562505 4.95105651629515 -2.92886848047812,-1.75807810440033 4.970295726276 -3.04508060049576,-1.82635182233307 4.98480775301221 -3.16333414877688,-1.89547153673235 4.99452189536827 -3.28305300592108,-1.9651005032975 4.99939082701909 -3.40365391369044,-2.0348994967025 4.99939082701909 -3.52454931658507,-2.10452846326765 4.99452189536827 -3.64515022435443,-2.17364817766693 4.98480775301221 -3.76486908149863,-2.24192189559967 4.970295726276 -3.88312262977975,-2.30901699437495 4.95105651629515 -3.99933474979739,-2.37460659341591 4.92718385456679 -4.11293926778441,-2.43837114678908 4.89879404629917 -4.22338271394867,-2.5 4.86602540378444 -4.33012701892219,-2.55919290347075 4.82903757255504 -4.43265213518105,-2.61566147532566 4.78801075360672 -4.53045857066461,-2.66913060635886 4.74314482547739 -4.62306982225067,-2.71933980033865 4.694658370459 -4.71003469723075,-2.76604444311898 4.64278760968654 -4.79092951147563,-2.80901699437495 4.58778525229247 -4.86536015358183,-2.84804809615642 4.5299192642332 -4.93296400494274,-2.88294759285893 4.46947156278589 -4.99341170639005,-2.9135454576426 4.4067366430758 -5.0464087627985,-2.93969262078591 4.34202014332567 -5.0916969778365,-2.96126169593832 4.275637355817 -5.12905571187275,-2.9781476007338 4.20791169081776 -5.1583029569103,-2.99026806874157 4.13917310096007 -5.17929622331126,-2.99756405025982 4.06975647374412 -5.19193323399196,-3.0 4.0 -5.19615242270663,-2.99756405025982 3.93024352625587 -5.19193323399196,-2.99026806874157 3.86082689903993 -5.17929622331126,-2.9781476007338 3.79208830918224 -5.1583029569103,-2.96126169593832 3.724362644183 -5.12905571187275,"
        + "-2.93969262078591 3.65797985667433 -5.0916969778365,-2.9135454576426 3.5932633569242 -5.0464087627985,-2.88294759285893 3.53052843721411 -4.99341170639005,-2.84804809615642 3.4700807357668 -4.93296400494274,-2.80901699437495 3.41221474770753 -4.86536015358183,-2.76604444311898 3.35721239031346 -4.79092951147563,-2.71933980033865 3.305341629541 -4.71003469723075,-2.66913060635886 3.25685517452261 -4.62306982225067,-2.61566147532566 3.21198924639328 -4.53045857066461,-2.55919290347075 3.17096242744496 -4.43265213518105,-2.5 3.13397459621556 -4.33012701892219,-2.43837114678908 3.10120595370083 -4.22338271394867,-2.37460659341591 3.07281614543321 -4.11293926778441,-2.30901699437495 3.04894348370485 -3.99933474979739,-2.24192189559967 3.029704273724 -3.88312262977975,-2.17364817766693 3.01519224698779 -3.76486908149863,-2.10452846326765 3.00547810463173 -3.64515022435443,-2.0348994967025 3.0006091729809 -3.52454931658507,-1.9651005032975 3.0006091729809 -3.40365391369044,-1.89547153673235 3.00547810463173 -3.28305300592108,-1.82635182233307 3.01519224698779 -3.16333414877688,-1.75807810440033 3.029704273724 -3.04508060049576,-1.69098300562505 3.04894348370485 -2.92886848047812,-1.62539340658409 3.07281614543321 -2.8152639624911,-1.56162885321092 3.10120595370083 -2.70482051632684,-1.5 3.13397459621556 -2.59807621135332,-1.44080709652925 3.17096242744496 -2.49555109509446,-1.38433852467434 3.21198924639328 -2.3977446596109,-1.33086939364114 3.25685517452261 -2.30513340802484,-1.28066019966135 3.305341629541 -2.21816853304476,-1.23395555688102 3.35721239031346 -2.13727371879988,-1.19098300562505 3.41221474770753 -2.06284307669368,-1.15195190384357 3.4700807357668 -1.99523922533277,-1.11705240714107 3.53052843721411 -1.93479152388545,-1.0864545423574 3.5932633569242 -1.88179446747701,-1.06030737921409 3.65797985667433 -1.83650625243901,-1.03873830406168 3.724362644183 -1.79914751840276,-1.02185239926619 3.79208830918224 -1.76990027336521,-1.00973193125843 3.86082689903993 -1.74890700696425,-1.00243594974018 3.93024352625587 -1.73626999628355,-1.0 4.0 -1.73205080756888)",
    )

    # OGRFeature(entities):27
    #   EntityHandle (String) = 1D7
    #   LINESTRING Z (-1.0 2.0 -1.73205080756888,-0.983288972510522 2.03582232791524 -1.70310645891042,-0.967858303543227 2.07387296203834 -1.67637975626429,-0.953782068732337 2.11396923855472 -1.65199900239256,-0.941127841688774 2.15591867344963 -1.6300812382226,-0.929956369609942 2.19951988653655 -1.61073168098672,-0.92032128166002 2.24456356819194 -1.59404321912206,-0.912268831520699 2.29083348415575 -1.58009596635534,-0.905837675348257 2.33810751357387 -1.56895687711326,-0.901058686202915 2.38615871529951 -1.56067942510471,-0.89795480584129 2.43475641733454 -1.55530334661776,-0.896540934583439 2.48366732418105 -1.5528544497638,-0.896823859783196 2.53265663678705 -1.55334449058452,-0.898802223245158 2.58148917971011 -1.55677111661648,-0.902466527744776 2.62993053008786 -1.56311787818422,-0.907799182620207 2.67774814299567 -1.5723543073677,-0.9147745882171 2.72471246778926 -1.58443606426492,-0.923359258780906 2.7705980500731 -1.59930514984767,-0.933511983206783 2.81518461400453 -1.61689018438853,-0.945184022875394 2.85825811973811 -1.63710675012253,-0.958319345624882 2.89961179093367 -1.65985779649846,-0.972854894735838 2.93904710739563 -1.68503410607454,-0.988720891637972 2.97637475807832 -1.71251481882177,-1.00584117088534 3.01141554988232 -1.74216801231798,-1.02413354579207 3.04400126787917 -1.77385133504753,-1.04351020297329 3.07397548283483 -1.80741268976625,-1.06387812389736 3.10119430215541 -1.84269096365129,-1.08513953142555 3.12552706065018 -1.87951680173053,-1.1071923591957 3.14685694779575 -1.917713419879,-1.12993074159648 3.16508156849045 -1.95709745347909,-1.15324552198012 3.18011343460661 -1.99747983767086,-1.17702477667387 3.1918803849814 -2.03866671496655,-1.20115435227475 3.20032593182975 -2.08046036587236,-1.22551841364822 3.2054095319166 -2.12266015804993,-1.25 3.20710678118655 -2.1650635094611,-1.27448158635178 3.2054095319166 -2.20746686087227,-1.29884564772525 3.20032593182975 -2.24966665304983,-1.32297522332613 3.1918803849814 -2.29146030395564,-1.34675447801988 3.18011343460661 -2.33264718125133,-1.37006925840352 3.16508156849045 -2.3730295654431,-1.3928076408043 3.14685694779575 -2.41241359904319,-1.41486046857445 3.12552706065018 -2.45061021719166,-1.43612187610264 3.10119430215541 -2.48743605527091,-1.45648979702671 3.07397548283483 -2.52271432915594,-1.47586645420793 3.04400126787917 -2.55627568387467,-1.49415882911466 3.01141554988232 -2.58795900660421,-1.51127910836203 2.97637475807832 -2.61761220010042,-1.52714510526416 2.93904710739563 -2.64509291284765,-1.54168065437512 2.89961179093367 -2.67026922242374,-1.55481597712461 2.85825811973811 -2.69302026879967,-1.56648801679322 2.81518461400453 -2.71323683453366,-1.57664074121909 2.7705980500731 -2.73082186907453,-1.5852254117829 2.72471246778926 -2.74569095465728,-1.59220081737979 2.67774814299567 -2.7577727115545,-1.59753347225522 2.62993053008785 -2.76700914073797,-1.60119777675484 2.58148917971011 -2.77335590230571,-1.6031761402168 2.53265663678705 -2.77678252833767,-1.60345906541656 2.48366732418105 -2.77727256915839,-1.60204519415871 2.43475641733454 -2.77482367230443,-1.59894131379708 2.38615871529951 -2.76944759381748,-1.59416232465174 2.33810751357387 -2.76117014180893,-1.5877311684793 2.29083348415575 -2.75003105256685,-1.57967871833998 2.24456356819194 -2.73608379980013,-1.57004363039006 2.19951988653655 -2.71939533793547,-1.55887215831123 2.15591867344963 -2.7000457806996,-1.54621793126766 2.11396923855472 -2.67812801652963,-1.53214169645677 2.07387296203834 -2.6537472626579,-1.51671102748948 2.03582232791524 -2.62702056001177,-1.5 2.0 -2.59807621135332)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-1.0 2.0 -1.73205080756888,-0.983288972510522 2.03582232791524 -1.70310645891042,-0.967858303543227 2.07387296203834 -1.67637975626429,-0.953782068732337 2.11396923855472 -1.65199900239256,-0.941127841688774 2.15591867344963 -1.6300812382226,-0.929956369609942 2.19951988653655 -1.61073168098672,-0.92032128166002 2.24456356819194 -1.59404321912206,-0.912268831520699 2.29083348415575 -1.58009596635534,-0.905837675348257 2.33810751357387 -1.56895687711326,-0.901058686202915 2.38615871529951 -1.56067942510471,-0.89795480584129 2.43475641733454 -1.55530334661776,-0.896540934583439 2.48366732418105 -1.5528544497638,-0.896823859783196 2.53265663678705 -1.55334449058452,-0.898802223245158 2.58148917971011 -1.55677111661648,-0.902466527744776 2.62993053008786 -1.56311787818422,-0.907799182620207 2.67774814299567 -1.5723543073677,-0.9147745882171 2.72471246778926 -1.58443606426492,-0.923359258780906 2.7705980500731 -1.59930514984767,-0.933511983206783 2.81518461400453 -1.61689018438853,-0.945184022875394 2.85825811973811 -1.63710675012253,-0.958319345624882 2.89961179093367 -1.65985779649846,-0.972854894735838 2.93904710739563 -1.68503410607454,-0.988720891637972 2.97637475807832 -1.71251481882177,-1.00584117088534 3.01141554988232 -1.74216801231798,-1.02413354579207 3.04400126787917 -1.77385133504753,-1.04351020297329 3.07397548283483 -1.80741268976625,-1.06387812389736 3.10119430215541 -1.84269096365129,-1.08513953142555 3.12552706065018 -1.87951680173053,-1.1071923591957 3.14685694779575 -1.917713419879,-1.12993074159648 3.16508156849045 -1.95709745347909,-1.15324552198012 3.18011343460661 -1.99747983767086,-1.17702477667387 3.1918803849814 -2.03866671496655,-1.20115435227475 3.20032593182975 -2.08046036587236,-1.22551841364822 3.2054095319166 -2.12266015804993,-1.25 3.20710678118655 -2.1650635094611,-1.27448158635178 3.2054095319166 -2.20746686087227,-1.29884564772525 3.20032593182975 -2.24966665304983,-1.32297522332613 3.1918803849814 -2.29146030395564,-1.34675447801988 3.18011343460661 -2.33264718125133,-1.37006925840352 3.16508156849045 -2.3730295654431,-1.3928076408043 3.14685694779575 -2.41241359904319,-1.41486046857445 3.12552706065018 -2.45061021719166,-1.43612187610264 3.10119430215541 -2.48743605527091,-1.45648979702671 3.07397548283483 -2.52271432915594,-1.47586645420793 3.04400126787917 -2.55627568387467,-1.49415882911466 3.01141554988232 -2.58795900660421,-1.51127910836203 2.97637475807832 -2.61761220010042,-1.52714510526416 2.93904710739563 -2.64509291284765,-1.54168065437512 2.89961179093367 -2.67026922242374,-1.55481597712461 2.85825811973811 -2.69302026879967,-1.56648801679322 2.81518461400453 -2.71323683453366,-1.57664074121909 2.7705980500731 -2.73082186907453,-1.5852254117829 2.72471246778926 -2.74569095465728,-1.59220081737979 2.67774814299567 -2.7577727115545,-1.59753347225522 2.62993053008785 -2.76700914073797,-1.60119777675484 2.58148917971011 -2.77335590230571,-1.6031761402168 2.53265663678705 -2.77678252833767,-1.60345906541656 2.48366732418105 -2.77727256915839,-1.60204519415871 2.43475641733454 -2.77482367230443,-1.59894131379708 2.38615871529951 -2.76944759381748,-1.59416232465174 2.33810751357387 -2.76117014180893,-1.5877311684793 2.29083348415575 -2.75003105256685,-1.57967871833998 2.24456356819194 -2.73608379980013,-1.57004363039006 2.19951988653655 -2.71939533793547,-1.55887215831123 2.15591867344963 -2.7000457806996,-1.54621793126766 2.11396923855472 -2.67812801652963,-1.53214169645677 2.07387296203834 -2.6537472626579,-1.51671102748948 2.03582232791524 -2.62702056001177,-1.5 2.0 -2.59807621135332)",
    )

    # OGRFeature(entities):28
    #   EntityHandle (String) = 1D8
    #   POLYGON Z ((-0.5 2.0 -0.866025403784439,-0.5 3.0 -0.866025403784439,-1.0 3.0 -1.73205080756888,-1.0 2.0 -1.73205080756888,-0.5 2.0 -0.866025403784439))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((-0.5 2.0 -0.866025403784439,-0.5 3.0 -0.866025403784439,-1.0 3.0 -1.73205080756888,-1.0 2.0 -1.73205080756888,-0.5 2.0 -0.866025403784439))",
    )

    # OGRFeature(entities):29
    #   EntityHandle (String) = 1D9
    #   POLYGON Z ((-1.5 4.0 -2.59807621135332,-2.0 4.0 -3.46410161513776,-2.0 3.0 -3.46410161513776,-1.5 3.0 -2.59807621135332,-1.5 4.0 -2.59807621135332))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON ((-1.5 4.0 -2.59807621135332,-2.0 4.0 -3.46410161513776,-2.0 3.0 -3.46410161513776,-1.5 3.0 -2.59807621135332,-1.5 4.0 -2.59807621135332))",
    )

    # OGRFeature(entities):30
    #   EntityHandle (String) = 1DB
    #   POLYGON Z ((-4.0 8.0 -6.92820323027551,-4.5 8.0 -7.79422863405995,-4.5 9.0 -7.79422863405995,-4.0 9.0 -6.92820323027551,-4.0 8.0 -6.92820323027551))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((-4.0 8.0 -6.92820323027551,-4.5 8.0 -7.79422863405995,-4.5 9.0 -7.79422863405995,-4.0 9.0 -6.92820323027551,-4.0 8.0 -6.92820323027551))",
    )

    # OGRFeature(entities):31
    #   EntityHandle (String) = 1DC
    #   LINESTRING Z (-1.0 2.0 -1.73205080756888,-1.07692307692308 2.15384615384615 -1.86528548507418,-1.15384615384615 2.30769230769231 -1.99852016257947,-1.23076923076923 2.46153846153846 -2.13175484008477,-1.30769230769231 2.61538461538461 -2.26498951759007,-1.38461538461538 2.76923076923077 -2.39822419509537,-1.46153846153846 2.92307692307692 -2.53145887260067,-1.53846153846154 3.07692307692308 -2.66469355010597,-1.61538461538461 3.23076923076923 -2.79792822761126,-1.69230769230769 3.38461538461538 -2.93116290511656,-1.76923076923077 3.53846153846154 -3.06439758262186,-1.84615384615384 3.69230769230769 -3.19763226012716,-1.92307692307692 3.84615384615385 -3.33086693763246,-2 4.0 -3.46410161513776,-2.07692307692307 4.15384615384615 -3.59733629264305,-2.15384615384615 4.30769230769231 -3.73057097014835,-2.23076923076923 4.46153846153846 -3.86380564765365,-2.30769230769231 4.61538461538462 -3.99704032515895,-2.38461538461538 4.76923076923077 -4.13027500266425,-2.46153846153846 4.92307692307692 -4.26350968016954,-2.53846153846154 5.07692307692308 -4.39674435767484,-2.61538461538461 5.23076923076923 -4.52997903518014,-2.69230769230769 5.38461538461538 -4.66321371268544,-2.76923076923077 5.53846153846154 -4.79644839019074,-2.84615384615384 5.69230769230769 -4.92968306769604,-2.92307692307692 5.84615384615385 -5.06291774520133,-3 6.0 -5.19615242270663,-3.07692307692308 6.15384615384615 -5.32938710021193,-3.15384615384615 6.30769230769231 -5.46262177771723,-3.23076923076923 6.46153846153846 -5.59585645522253,-3.30769230769231 6.61538461538462 -5.72909113272783,-3.38461538461538 6.76923076923077 -5.86232581023313,-3.46153846153846 6.92307692307692 -5.99556048773842,-3.53846153846154 7.07692307692308 -6.12879516524372,-3.61538461538461 7.23076923076923 -6.26202984274902,-3.69230769230769 7.38461538461539 -6.39526452025432,-3.76923076923077 7.53846153846154 -6.52849919775962,-3.84615384615384 7.69230769230769 -6.66173387526491,-3.92307692307692 7.84615384615385 -6.79496855277021,-4 8.0 -6.92820323027551)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-1.0 2.0 -1.73205080756888,-1.07692307692308 2.15384615384615 -1.86528548507418,-1.15384615384615 2.30769230769231 -1.99852016257947,-1.23076923076923 2.46153846153846 -2.13175484008477,-1.30769230769231 2.61538461538461 -2.26498951759007,-1.38461538461538 2.76923076923077 -2.39822419509537,-1.46153846153846 2.92307692307692 -2.53145887260067,-1.53846153846154 3.07692307692308 -2.66469355010597,-1.61538461538461 3.23076923076923 -2.79792822761126,-1.69230769230769 3.38461538461538 -2.93116290511656,-1.76923076923077 3.53846153846154 -3.06439758262186,-1.84615384615384 3.69230769230769 -3.19763226012716,-1.92307692307692 3.84615384615385 -3.33086693763246,-2 4.0 -3.46410161513776,-2.07692307692307 4.15384615384615 -3.59733629264305,-2.15384615384615 4.30769230769231 -3.73057097014835,-2.23076923076923 4.46153846153846 -3.86380564765365,-2.30769230769231 4.61538461538462 -3.99704032515895,-2.38461538461538 4.76923076923077 -4.13027500266425,-2.46153846153846 4.92307692307692 -4.26350968016954,-2.53846153846154 5.07692307692308 -4.39674435767484,-2.61538461538461 5.23076923076923 -4.52997903518014,-2.69230769230769 5.38461538461538 -4.66321371268544,-2.76923076923077 5.53846153846154 -4.79644839019074,-2.84615384615384 5.69230769230769 -4.92968306769604,-2.92307692307692 5.84615384615385 -5.06291774520133,-3 6.0 -5.19615242270663,-3.07692307692308 6.15384615384615 -5.32938710021193,-3.15384615384615 6.30769230769231 -5.46262177771723,-3.23076923076923 6.46153846153846 -5.59585645522253,-3.30769230769231 6.61538461538462 -5.72909113272783,-3.38461538461538 6.76923076923077 -5.86232581023313,-3.46153846153846 6.92307692307692 -5.99556048773842,-3.53846153846154 7.07692307692308 -6.12879516524372,-3.61538461538461 7.23076923076923 -6.26202984274902,-3.69230769230769 7.38461538461539 -6.39526452025432,-3.76923076923077 7.53846153846154 -6.52849919775962,-3.84615384615384 7.69230769230769 -6.66173387526491,-3.92307692307692 7.84615384615385 -6.79496855277021,-4 8.0 -6.92820323027551)",
    )

    # OGRFeature(entities):32
    #   EntityHandle (String) = 1DD
    #   LINESTRING Z (-4 1.0 -6.92820323027551,-3.81418685412768 0.987348067229724 -6.60636542091045,-3.62887944840607 0.975707614760869 -6.28540357918185,-3.44458352298589 0.966090122894857 -5.96619367272616,-3.26180481801783 0.959507071933107 -5.64961166917985,-3.08104907365262 0.956969942177043 -5.33653353617937,-2.90282203004096 0.959490213928084 -5.02783524136118,-2.72762942733357 0.968079367487651 -4.72439275236174,-2.55597700568115 0.983748883157167 -4.42708203681751,-2.38837050523441 1.00751024123805 -4.13677906236495,-2.22531566614407 1.04037492203173 -3.85435979664051,-2.06731822856083 1.08335440583961 -3.58070020728065,-1.91488393263541 1.13746017296313 -3.31667626192183,-1.76851851851852 1.2037037037037 -3.06316392820052,-1.62872772636086 1.28309647836275 -2.82103917375315,-1.49601729631315 1.37664997724169 -2.59117796621621,-1.3708929685261 1.48537568064195 -2.37445627322614,-1.25386048315042 1.61028506886495 -2.1717500624194,-1.14542558033682 1.75238962221211 -1.98393530143245,-1.04609400023601 1.91270082098484 -1.81188795790174,-0.956350410492422 2.09218800047202 -1.65644750081223,-0.876194811106054 2.29085116067364 -1.5176139301639,-0.805142534432475 2.50772096630085 -1.39454777697182,-0.742687840320977 2.74178593705221 -1.28637307359953,-0.688324988620847 2.99203459262631 -1.19221385241058,-0.641548239181376 3.25745545272172 -1.11119414576849,-0.601851851851852 3.53703703703703 -1.04243798603683,-0.568730086481566 3.82976786527082 -0.985069405579114,-0.541677202919806 4.13463645712166 -0.938212436758902,-0.520187461015863 4.45063133228814 -0.90099111193973,-0.503755120619026 4.77674101046882 -0.872529463485141,-0.491874441578583 5.11195401136229 -0.851951523758677,-0.484039683743826 5.45525885466714 -0.838381325123878,-0.479745106964042 5.80564406008193 -0.830942899944286,-0.478484971088521 6.16209814730525 -0.828760280583445,-0.479753535966554 6.52360963603567 -0.830957499404894,-0.483045061447428 6.88916704597178 -0.836658588772177,-0.487853807380435 7.25775889681216 -0.844987581048834,-0.493674033614862 7.62837370825537 -0.855068508598407,-0.5 8.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-4 1.0 -6.92820323027551,-3.81418685412768 0.987348067229724 -6.60636542091045,-3.62887944840607 0.975707614760869 -6.28540357918185,-3.44458352298589 0.966090122894857 -5.96619367272616,-3.26180481801783 0.959507071933107 -5.64961166917985,-3.08104907365262 0.956969942177043 -5.33653353617937,-2.90282203004096 0.959490213928084 -5.02783524136118,-2.72762942733357 0.968079367487651 -4.72439275236174,-2.55597700568115 0.983748883157167 -4.42708203681751,-2.38837050523441 1.00751024123805 -4.13677906236495,-2.22531566614407 1.04037492203173 -3.85435979664051,-2.06731822856083 1.08335440583961 -3.58070020728065,-1.91488393263541 1.13746017296313 -3.31667626192183,-1.76851851851852 1.2037037037037 -3.06316392820052,-1.62872772636086 1.28309647836275 -2.82103917375315,-1.49601729631315 1.37664997724169 -2.59117796621621,-1.3708929685261 1.48537568064195 -2.37445627322614,-1.25386048315042 1.61028506886495 -2.1717500624194,-1.14542558033682 1.75238962221211 -1.98393530143245,-1.04609400023601 1.91270082098484 -1.81188795790174,-0.956350410492422 2.09218800047202 -1.65644750081223,-0.876194811106054 2.29085116067364 -1.5176139301639,-0.805142534432475 2.50772096630085 -1.39454777697182,-0.742687840320977 2.74178593705221 -1.28637307359953,-0.688324988620847 2.99203459262631 -1.19221385241058,-0.641548239181376 3.25745545272172 -1.11119414576849,-0.601851851851852 3.53703703703703 -1.04243798603683,-0.568730086481566 3.82976786527082 -0.985069405579114,-0.541677202919806 4.13463645712166 -0.938212436758902,-0.520187461015863 4.45063133228814 -0.90099111193973,-0.503755120619026 4.77674101046882 -0.872529463485141,-0.491874441578583 5.11195401136229 -0.851951523758677,-0.484039683743826 5.45525885466714 -0.838381325123878,-0.479745106964042 5.80564406008193 -0.830942899944286,-0.478484971088521 6.16209814730525 -0.828760280583445,-0.479753535966554 6.52360963603567 -0.830957499404894,-0.483045061447428 6.88916704597178 -0.836658588772177,-0.487853807380435 7.25775889681216 -0.844987581048834,-0.493674033614862 7.62837370825537 -0.855068508598407,-0.5 8.0 -0.866025403784439)",
    )

    # OGRFeature(entities):33
    #   EntityHandle (String) = 1DE
    #   POINT Z (-3.5 7.0 -6.06217782649107)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (-3.5 7.0 -6.06217782649107)")

    # OGRFeature(entities):34
    #   EntityHandle (String) = 1DF
    #   POINT Z (1.0 -2.0 -5.19615242270663)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (1.0 -2.0 -5.19615242270663)")

    # OGRFeature(entities):35
    #   EntityHandle (String) = 1E0
    #   LINESTRING Z (0 0 0,0.25 -0.5 -1.29903810567666)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (0 0 0,0.25 -0.5 -1.29903810567666)"
    )

    # OGRFeature(entities):36
    #   EntityHandle (String) = 1E1
    #   LINESTRING Z (0.25 -0.5 -1.29903810567666,-0.25 -0.5 -2.1650635094611,1.0 -1.0 -1.73205080756888,0.25 -0.5 -1.29903810567666)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (0.25 -0.5 -1.29903810567666,-0.25 -0.5 -2.1650635094611,1.0 -1.0 -1.73205080756888,0.25 -0.5 -1.29903810567666)",
    )

    # OGRFeature(entities):37
    #   EntityHandle (String) = 1E2
    #   LINESTRING Z (0.25 -0.5 -1.29903810567666,1 -1 -1.73205080756888,0.5 -1.0 -2.59807621135332,0.25 -0.5 -1.29903810567666)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (0.25 -0.5 -1.29903810567666,1 -1 -1.73205080756888,0.5 -1.0 -2.59807621135332,0.25 -0.5 -1.29903810567666)",
    )

    # OGRFeature(entities):38
    #   EntityHandle (String) = 1E7
    #   LINESTRING Z (2.78885438199983 -2.89442719099992 -5.19615242270663,2.76889880091653 -2.9234444547489 -5.33123525325721,2.74032532268425 -2.94796278993866 -5.46565997383582,2.70327315441675 -2.96786274570473 -5.59877168071614,2.65792281055334 -2.98304737146295 -5.72992186704258,2.60449523341074 -2.99344268924297 -5.85847158229053,2.54325071677425 -2.99899805410152 -5.98379454516908,2.47448763777266 -2.99968640085941 -6.1052801948005,2.39854100321483 -2.99550437596044 -6.22233666531148,2.31578081747018 -2.98647235380955 -6.33439366934417,2.22661027984464 -2.97263433751074 -6.44090527643885,2.13146382023413 -2.95405774448845 -6.54135257275227,2.03080498262578 -2.93083307803656 -6.63524618915372,1.92512416675824 -2.90307348639549 -6.72212868538223,1.81493623894341 -2.87091421150532 -6.80157677864958,1.70077802368955 -2.8345119301207 -6.87320340583148,1.58320568834623 -2.79404399049737 -6.93665960920016,1.46279203351292 -2.74970754836936 -6.99163623651142,1.34012370241203 -2.70171860642604 -7.03786544716322,1.21579832282211 -2.65031096196872 -7.07512201708822,1.09042159549536 -2.59573506787371 -7.10322443602275,0.964604343244512 -2.5382568124111 -7.12203579180661,0.838959535075416 -2.47815622386382 -7.13146443740534,0.714099299863701 -2.41572610625796 -7.13146443740534,0.590631944124412 -2.35127061285105 -7.12203579180661,0.469158988403815 -2.28510376432794 -7.10322443602275,0.350272236731764 -2.21754791892355 -7.07512201708822,0.234550893411949 -2.148932201926 -7.03786544716322,0.122558741196756 -2.07959090221128 -6.99163623651142,0.014841394594365 -2.00986184362144 -6.93665960920016,-0.088076358310285 -1.94008473912078 -6.87320340583148,-0.185693112570302 -1.87059953574847 -6.80157677864958,-0.277533289171331 -1.8017447584307 -6.72212868538223,-0.363149452004714 -1.73385586072131 -6.63524618915372,-0.442124487731234 -1.66726359050577 -6.54135257275227,-0.514073637915374 -1.60229237863074 -6.44090527643885,-0.57864637352974 -1.53925875830959 -6.33439366934417,-0.635528102697248 -1.47846982300441 -6.22233666531148,-0.684441703351125 -1.42022173029752 -6.1052801948005,-0.725148873345763 -1.36479825904151 -5.98379454516908,-0.757451291440819 -1.31246942681719 -5.85847158229053,-0.781191583502361 -1.2634901744351 -5.72992186704258,-0.796254089213832 -1.21809912388944 -5.59877168071614,-0.802565425561482 -1.1765174158158 -5.46565997383582,-0.80009484434904 -1.13894763211612 -5.33123525325721,-0.788854381999831 -1.10557280900008 -5.19615242270663,-0.768898800916532 -1.0765555452511 -5.06106959215606,-0.740325322684253 -1.05203721006134 -4.92664487157745,-0.703273154416746 -1.03213725429527 -4.79353316469713,
    # -0.65792281055334 -1.01695262853705 -4.66238297837069,-0.604495233410736 -1.00655731075703 -4.53383326312274,-0.543250716774252 -1.00100194589848 -4.40851030024419,-0.474487637772662 -1.00031359914059 -4.28702465061277,-0.398541003214825 -1.00449562403956 -4.16996818010179,-0.315780817470174 -1.01352764619045 -4.0579111760691,-0.226610279844633 -1.02736566248926 -3.95139956897441,-0.131463820234124 -1.04594225551155 -3.85095227266099,-0.03080498262578 -1.06916692196344 -3.75705865625955,0.074875833241763 -1.09692651360451 -3.67017616003104,0.185063761056595 -1.12908578849468 -3.59072806676368,0.299221976310455 -1.1654880698793 -3.51910143958179,0.416794311653771 -1.20595600950263 -3.45564523621311,0.537207966487078 -1.25029245163064 -3.40066860890185,0.659876297587969 -1.29828139357396 -3.35443939825005,0.784201677177895 -1.34968903803128 -3.31718282832505,0.909578404504639 -1.40426493212629 -3.28908040939052,1.03539565675549 -1.4617431875889 -3.27026905360665,1.16104046492459 -1.52184377613619 -3.26084040800793,1.2859007001363 -1.58427389374204 -3.26084040800793,1.40936805587559 -1.64872938714895 -3.27026905360665,1.53084101159619 -1.71489623567206 -3.28908040939052,1.64972776326824 -1.78245208107646 -3.31718282832505,1.76544910658805 -1.851067798074 -3.35443939825005,1.87744125880325 -1.92040909778872 -3.40066860890185,1.98515860540564 -1.99013815637856 -3.45564523621311,2.08807635831029 -2.05991526087922 -3.51910143958179,2.1856931125703 -2.12940046425153 -3.59072806676368,2.27753328917133 -2.1982552415693 -3.67017616003104,2.36314945200472 -2.26614413927869 -3.75705865625955,2.44212448773124 -2.33273640949423 -3.85095227266099,2.51407363791538 -2.39770762136926 -3.95139956897441,2.57864637352974 -2.46074124169041 -4.0579111760691,2.63552810269725 -2.52153017699559 -4.16996818010179,2.68444170335113 -2.57977826970249 -4.28702465061277,2.72514887334576 -2.63520174095849 -4.40851030024419,2.75745129144082 -2.68753057318281 -4.53383326312274,2.78119158350236 -2.7365098255649 -4.66238297837069,2.79625408921383 -2.78190087611056 -4.79353316469713,2.80256542556148 -2.82348258418421 -4.92664487157744,2.80009484434904 -2.86105236788389 -5.06106959215606,2.78885438199983 -2.89442719099992 -5.19615242270663)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2.78885438199983 -2.89442719099992 -5.19615242270663,2.76889880091653 -2.9234444547489 -5.33123525325721,2.74032532268425 -2.94796278993866 -5.46565997383582,2.70327315441675 -2.96786274570473 -5.59877168071614,2.65792281055334 -2.98304737146295 -5.72992186704258,2.60449523341074 -2.99344268924297 -5.85847158229053,2.54325071677425 -2.99899805410152 -5.98379454516908,2.47448763777266 -2.99968640085941 -6.1052801948005,2.39854100321483 -2.99550437596044 -6.22233666531148,2.31578081747018 -2.98647235380955 -6.33439366934417,2.22661027984464 -2.97263433751074 -6.44090527643885,2.13146382023413 -2.95405774448845 -6.54135257275227,2.03080498262578 -2.93083307803656 -6.63524618915372,1.92512416675824 -2.90307348639549 -6.72212868538223,1.81493623894341 -2.87091421150532 -6.80157677864958,1.70077802368955 -2.8345119301207 -6.87320340583148,1.58320568834623 -2.79404399049737 -6.93665960920016,1.46279203351292 -2.74970754836936 -6.99163623651142,1.34012370241203 -2.70171860642604 -7.03786544716322,1.21579832282211 -2.65031096196872 -7.07512201708822,1.09042159549536 -2.59573506787371 -7.10322443602275,0.964604343244512 -2.5382568124111 -7.12203579180661,0.838959535075416 -2.47815622386382 -7.13146443740534,0.714099299863701 -2.41572610625796 -7.13146443740534,0.590631944124412 -2.35127061285105 -7.12203579180661,0.469158988403815 -2.28510376432794 -7.10322443602275,0.350272236731764 -2.21754791892355 -7.07512201708822,0.234550893411949 -2.148932201926 -7.03786544716322,0.122558741196756 -2.07959090221128 -6.99163623651142,0.014841394594365 -2.00986184362144 -6.93665960920016,-0.088076358310285 -1.94008473912078 -6.87320340583148,-0.185693112570302 -1.87059953574847 -6.80157677864958,-0.277533289171331 -1.8017447584307 -6.72212868538223,-0.363149452004714 -1.73385586072131 -6.63524618915372,-0.442124487731234 -1.66726359050577 -6.54135257275227,-0.514073637915374 -1.60229237863074 -6.44090527643885,-0.57864637352974 -1.53925875830959 -6.33439366934417,-0.635528102697248 -1.47846982300441 -6.22233666531148,-0.684441703351125 -1.42022173029752 -6.1052801948005,-0.725148873345763 -1.36479825904151 -5.98379454516908,-0.757451291440819 -1.31246942681719 -5.85847158229053,-0.781191583502361 -1.2634901744351 -5.72992186704258,-0.796254089213832 -1.21809912388944 -5.59877168071614,-0.802565425561482 -1.1765174158158 -5.46565997383582,-0.80009484434904 -1.13894763211612 -5.33123525325721,-0.788854381999831 -1.10557280900008 -5.19615242270663,-0.768898800916532 -1.0765555452511 -5.06106959215606,-0.740325322684253 -1.05203721006134 -4.92664487157745,-0.703273154416746 -1.03213725429527 -4.79353316469713,-0.65792281055334 -1.01695262853705 -4.66238297837069,-0.604495233410736 -1.00655731075703 -4.53383326312274,-0.543250716774252 -1.00100194589848 -4.40851030024419,-0.474487637772662 -1.00031359914059 -4.28702465061277,-0.398541003214825 -1.00449562403956 -4.16996818010179,-0.315780817470174 -1.01352764619045 -4.0579111760691,"
        + "-0.226610279844633 -1.02736566248926 -3.95139956897441,-0.131463820234124 -1.04594225551155 -3.85095227266099,-0.03080498262578 -1.06916692196344 -3.75705865625955,0.074875833241763 -1.09692651360451 -3.67017616003104,0.185063761056595 -1.12908578849468 -3.59072806676368,0.299221976310455 -1.1654880698793 -3.51910143958179,0.416794311653771 -1.20595600950263 -3.45564523621311,0.537207966487078 -1.25029245163064 -3.40066860890185,0.659876297587969 -1.29828139357396 -3.35443939825005,0.784201677177895 -1.34968903803128 -3.31718282832505,0.909578404504639 -1.40426493212629 -3.28908040939052,1.03539565675549 -1.4617431875889 -3.27026905360665,1.16104046492459 -1.52184377613619 -3.26084040800793,1.2859007001363 -1.58427389374204 -3.26084040800793,1.40936805587559 -1.64872938714895 -3.27026905360665,1.53084101159619 -1.71489623567206 -3.28908040939052,1.64972776326824 -1.78245208107646 -3.31718282832505,1.76544910658805 -1.851067798074 -3.35443939825005,1.87744125880325 -1.92040909778872 -3.40066860890185,1.98515860540564 -1.99013815637856 -3.45564523621311,2.08807635831029 -2.05991526087922 -3.51910143958179,2.1856931125703 -2.12940046425153 -3.59072806676368,2.27753328917133 -2.1982552415693 -3.67017616003104,2.36314945200472 -2.26614413927869 -3.75705865625955,2.44212448773124 -2.33273640949423 -3.85095227266099,2.51407363791538 -2.39770762136926 -3.95139956897441,2.57864637352974 -2.46074124169041 -4.0579111760691,2.63552810269725 -2.52153017699559 -4.16996818010179,2.68444170335113 -2.57977826970249 -4.28702465061277,2.72514887334576 -2.63520174095849 -4.40851030024419,2.75745129144082 -2.68753057318281 -4.53383326312274,2.78119158350236 -2.7365098255649 -4.66238297837069,2.79625408921383 -2.78190087611056 -4.79353316469713,2.80256542556148 -2.82348258418421 -4.92664487157744,2.80009484434904 -2.86105236788389 -5.06106959215606,2.78885438199983 -2.89442719099992 -5.19615242270663)",
    )

    # OGRFeature(entities):39
    #   EntityHandle (String) = 1E8
    #   LINESTRING Z (2.0 -2.0 -3.46410161513775,1.0 -2.0 -5.19615242270663)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (2.0 -2.0 -3.46410161513775,1.0 -2.0 -5.19615242270663)"
    )

    # OGRFeature(entities):40
    #   EntityHandle (String) = 1E9
    #   LINESTRING Z (1.0 -2.0 -5.19615242270663,0.0 -2.0 -6.92820323027551)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (1.0 -2.0 -5.19615242270663,0.0 -2.0 -6.92820323027551)"
    )

    # OGRFeature(entities):41
    #   EntityHandle (String) = 1EA
    #   LINESTRING Z (0.25 -1.5 -4.76313972081441,1.0 -2.0 -5.19615242270663)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (0.25 -1.5 -4.76313972081441,1.0 -2.0 -5.19615242270663)"
    )

    # OGRFeature(entities):42
    #   EntityHandle (String) = 1EB
    #   LINESTRING Z (1.0 -2.0 -5.19615242270663,1.75 -2.5 -5.62916512459885)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (1.0 -2.0 -5.19615242270663,1.75 -2.5 -5.62916512459885)"
    )

    # OGRFeature(entities):43
    #   EntityHandle (String) = 1EC
    #   LINESTRING Z (2.0 -2.0 -3.46410161513775,2.04988140556792 -2.03487823687206 -3.49852624302284,2.09464789446162 -2.06958655048003 -3.54122153501056,2.13408136884713 -2.10395584540888 -3.59197948393006,2.16798971280107 -2.1378186779085 -3.65055280215638,2.19620772828016 -2.17101007166283 -3.7166561263709,2.21859793994945 -2.2033683215379 -3.78996740782271,2.23505126494835 -2.23473578139294 -3.87012948131781,2.24548754433133 -2.2649596321166 -3.956751805292,2.2498559335943 -2.29389262614624 -4.04941236449012,2.24813515038388 -2.32139380484327 -4.14765972598196,2.2403335781829 -2.3473291852295 -4.25101523849813,2.2264892254669 -2.3715724127387 -4.35897536437091,2.2066695405307 -2.39400537680336 -4.47101413271915,2.18097108288703 -2.41451878627752 -4.58658570192556,2.14951905283833 -2.43301270189222 -4.70512701892219,2.11246668151345 -2.44939702314958 -4.82606056232836,2.069994484341 -2.46359192728339 -4.94879715607678,2.02230938159631 -2.47552825814758 -5.07273883982016,1.96964369030667 -2.485147863138 -5.19728178213388,1.91225399242609 -2.4924038765061 -5.32181922232198,1.85041988479386 -2.49726094768414 -5.44574442649435,1.78444261696682 -2.49969541350955 -5.56845364351315,1.71464362356182 -2.49969541350955 -5.68934904640778,1.64136295825855 -2.49726094768414 -5.80784164492769,1.56495763709223 -2.4924038765061 -5.92335415504372,1.48579989910733 -2.485147863138 -6.03532381141787,1.40427539284642 -2.47552825814758 -6.14320510913943,1.32078129750918 -2.46359192728339 -6.24647246137009,1.2357243879353 -2.44939702314958 -6.34462275995019,1.14951905283833 -2.43301270189222 -6.43717782649107,1.06258527594554 -2.41451878627752 -6.52368674201215,0.975346589879385 -2.39400537680336 -6.60372804377285,0.888228012749189 -2.3715724127387 -6.67691177859673,0.801653977505599 -2.3473291852295 -6.74288140268412,0.716046264145928 -2.32139380484327 -6.80131551865772,0.631821944844409 -2.29389262614623 -6.85192944137827,0.549391352018479 -2.2649596321166 -6.89447658490197,0.469156079230493 -2.23473578139294 -6.92874966382241,0.391507024664251 -2.2033683215379 -6.9545817031442,0.316822486708345 -2.17101007166283 -6.97184685176839,0.245466320924432 -2.1378186779085 -6.98046099562637,0.177786167379516 -2.10395584540888 -6.98038216747516,0.114111756978481 -2.06958655048003 -6.97161075135758,0.054753305048271 -2.03487823687206 -6.95418948073126,0.0 -2.0 -6.92820323027551,-0.049881405567917 -1.96512176312794 -6.89377860239042,-0.094647894461618 -1.93041344951997 -6.8510833104027,-0.134081368847123 -1.89604415459112 -6.8003253614832,-0.167989712801067 -1.8621813220915 -6.74175204325688,-0.196207728280158 -1.82898992833716 -6.67564871904237,-0.218597939949449 -1.7966316784621 -6.60233743759055,-0.235051264948343 -1.76526421860705 -6.52217536409545,
    # -0.245487544331328 -1.7350403678834 -6.43555304012126,-0.2498559335943 -1.70610737385376 -6.34289248092314,-0.248135150383881 -1.67860619515673 -6.2446451194313,-0.240333578182897 -1.6526708147705 -6.14128960691513,-0.226489225466902 -1.6284275872613 -6.03332948104236,-0.206669540530698 -1.60599462319664 -5.92129071269411,-0.180971082887026 -1.58548121372248 -5.8057191434877,-0.149519052838327 -1.56698729810778 -5.68717782649107,-0.112466681513451 -1.55060297685042 -5.5662442830849,-0.069994484341001 -1.5364080727166 -5.44350768933648,-0.022309381596311 -1.52447174185242 -5.3195660055931,0.030356309693336 -1.514852136862 -5.19502306327939,0.087746007573915 -1.50759612349389 -5.07048562309128,0.149580115206143 -1.50273905231586 -4.94656041891892,0.215557383033179 -1.50030458649045 -4.82385120190011,0.28535637643818 -1.50030458649045 -4.70295579900548,0.35863704174145 -1.50273905231586 -4.58446320048557,0.435042362907775 -1.50759612349389 -4.46895069036954,0.514200100892672 -1.514852136862 -4.35698103399539,0.595724607153583 -1.52447174185242 -4.24909973627383,0.679218702490823 -1.5364080727166 -4.14583238404317,0.764275612064703 -1.55060297685042 -4.04768208546307,0.850480947161672 -1.56698729810778 -3.95512701892219,0.937414724054466 -1.58548121372248 -3.86861810340111,1.02465341012062 -1.60599462319664 -3.78857680164041,1.11177198725081 -1.6284275872613 -3.71539306681653,1.1983460224944 -1.6526708147705 -3.64942344272914,1.28395373585407 -1.67860619515673 -3.59098932675554,1.36817805515559 -1.70610737385376 -3.54037540403499,1.45060864798152 -1.7350403678834 -3.49782826051129,1.53084392076951 -1.76526421860705 -3.46355518159085,1.60849297533575 -1.7966316784621 -3.43772314226906,1.68317751329166 -1.82898992833716 -3.42045799364487,1.75453367907557 -1.8621813220915 -3.41184384978689,1.82221383262049 -1.89604415459112 -3.4119226779381,1.88588824302152 -1.93041344951997 -3.42069409405568,1.94524669495173 -1.96512176312794 -3.438115364682,2.0 -2.0 -3.46410161513775)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2.0 -2.0 -3.46410161513775,2.04988140556792 -2.03487823687206 -3.49852624302284,2.09464789446162 -2.06958655048003 -3.54122153501056,2.13408136884713 -2.10395584540888 -3.59197948393006,2.16798971280107 -2.1378186779085 -3.65055280215638,2.19620772828016 -2.17101007166283 -3.7166561263709,2.21859793994945 -2.2033683215379 -3.78996740782271,2.23505126494835 -2.23473578139294 -3.87012948131781,2.24548754433133 -2.2649596321166 -3.956751805292,2.2498559335943 -2.29389262614624 -4.04941236449012,2.24813515038388 -2.32139380484327 -4.14765972598196,2.2403335781829 -2.3473291852295 -4.25101523849813,2.2264892254669 -2.3715724127387 -4.35897536437091,2.2066695405307 -2.39400537680336 -4.47101413271915,2.18097108288703 -2.41451878627752 -4.58658570192556,2.14951905283833 -2.43301270189222 -4.70512701892219,2.11246668151345 -2.44939702314958 -4.82606056232836,2.069994484341 -2.46359192728339 -4.94879715607678,2.02230938159631 -2.47552825814758 -5.07273883982016,1.96964369030667 -2.485147863138 -5.19728178213388,1.91225399242609 -2.4924038765061 -5.32181922232198,1.85041988479386 -2.49726094768414 -5.44574442649435,1.78444261696682 -2.49969541350955 -5.56845364351315,1.71464362356182 -2.49969541350955 -5.68934904640778,1.64136295825855 -2.49726094768414 -5.80784164492769,1.56495763709223 -2.4924038765061 -5.92335415504372,1.48579989910733 -2.485147863138 -6.03532381141787,1.40427539284642 -2.47552825814758 -6.14320510913943,1.32078129750918 -2.46359192728339 -6.24647246137009,1.2357243879353 -2.44939702314958 -6.34462275995019,1.14951905283833 -2.43301270189222 -6.43717782649107,1.06258527594554 -2.41451878627752 -6.52368674201215,0.975346589879385 -2.39400537680336 -6.60372804377285,0.888228012749189 -2.3715724127387 -6.67691177859673,0.801653977505599 -2.3473291852295 -6.74288140268412,0.716046264145928 -2.32139380484327 -6.80131551865772,0.631821944844409 -2.29389262614623 -6.85192944137827,0.549391352018479 -2.2649596321166 -6.89447658490197,0.469156079230493 -2.23473578139294 -6.92874966382241,0.391507024664251 -2.2033683215379 -6.9545817031442,0.316822486708345 -2.17101007166283 -6.97184685176839,0.245466320924432 -2.1378186779085 -6.98046099562637,0.177786167379516 -2.10395584540888 -6.98038216747516,0.114111756978481 -2.06958655048003 -6.97161075135758,0.054753305048271 -2.03487823687206 -6.95418948073126,0.0 -2.0 -6.92820323027551,-0.049881405567917 -1.96512176312794 -6.89377860239042,-0.094647894461618 -1.93041344951997 -6.8510833104027,-0.134081368847123 -1.89604415459112 -6.8003253614832,-0.167989712801067 -1.8621813220915 -6.74175204325688,-0.196207728280158 -1.82898992833716 -6.67564871904237,-0.218597939949449 -1.7966316784621 -6.60233743759055,"
        + "-0.235051264948343 -1.76526421860705 -6.52217536409545,-0.245487544331328 -1.7350403678834 -6.43555304012126,-0.2498559335943 -1.70610737385376 -6.34289248092314,-0.248135150383881 -1.67860619515673 -6.2446451194313,-0.240333578182897 -1.6526708147705 -6.14128960691513,-0.226489225466902 -1.6284275872613 -6.03332948104236,-0.206669540530698 -1.60599462319664 -5.92129071269411,-0.180971082887026 -1.58548121372248 -5.8057191434877,-0.149519052838327 -1.56698729810778 -5.68717782649107,-0.112466681513451 -1.55060297685042 -5.5662442830849,-0.069994484341001 -1.5364080727166 -5.44350768933648,-0.022309381596311 -1.52447174185242 -5.3195660055931,0.030356309693336 -1.514852136862 -5.19502306327939,0.087746007573915 -1.50759612349389 -5.07048562309128,0.149580115206143 -1.50273905231586 -4.94656041891892,0.215557383033179 -1.50030458649045 -4.82385120190011,0.28535637643818 -1.50030458649045 -4.70295579900548,0.35863704174145 -1.50273905231586 -4.58446320048557,0.435042362907775 -1.50759612349389 -4.46895069036954,0.514200100892672 -1.514852136862 -4.35698103399539,0.595724607153583 -1.52447174185242 -4.24909973627383,0.679218702490823 -1.5364080727166 -4.14583238404317,0.764275612064703 -1.55060297685042 -4.04768208546307,0.850480947161672 -1.56698729810778 -3.95512701892219,0.937414724054466 -1.58548121372248 -3.86861810340111,1.02465341012062 -1.60599462319664 -3.78857680164041,1.11177198725081 -1.6284275872613 -3.71539306681653,1.1983460224944 -1.6526708147705 -3.64942344272914,1.28395373585407 -1.67860619515673 -3.59098932675554,1.36817805515559 -1.70610737385376 -3.54037540403499,1.45060864798152 -1.7350403678834 -3.49782826051129,1.53084392076951 -1.76526421860705 -3.46355518159085,1.60849297533575 -1.7966316784621 -3.43772314226906,1.68317751329166 -1.82898992833716 -3.42045799364487,1.75453367907557 -1.8621813220915 -3.41184384978689,1.82221383262049 -1.89604415459112 -3.4119226779381,1.88588824302152 -1.93041344951997 -3.42069409405568,1.94524669495173 -1.96512176312794 -3.438115364682,2.0 -2.0 -3.46410161513775)",
    )

    # OGRFeature(entities):44
    #   EntityHandle (String) = 1ED
    #   LINESTRING Z (0.5 -1.0 -2.59807621135332,0.543577773425908 -1.01791116395762 -2.58464338569351,0.587546417985528 -1.03693648101917 -2.57439309093773,0.631694860183701 -1.05698461927736 -2.56737453409618,0.675811163398452 -1.07795933672482 -2.56362140807291,0.719683545292469 -1.09975994326827 -2.56315172992158,0.763101394483934 -1.12228178409597 -2.5659677543537,0.805856281596113 -1.14541674207787 -2.57205596291479,0.847742959832148 -1.16905375678694 -2.58138712888039,0.888560350271717 -1.19307935764976 -2.59391645756022,0.928112507159618 -1.21737820866727 -2.60958380133721,0.96620955855235 -1.24183366209053 -2.62831394840886,1.00266861780709 -1.26632831839353 -2.65001698384495,1.03731466153743 -1.29074458985506 -2.67458872122829,1.06998136982112 -1.31496526504393 -2.7019112028064,1.10051192462654 -1.33887407149783 -2.73185326575312,1.12875976262485 -1.36235623389463 -2.76427117182176,1.15458927877392 -1.38529902503655 -2.79900929736714,1.17787647729662 -1.40759230700227 -2.83590088042404,1.19850956692819 -1.42912905986906 -2.8747688212557,1.21638949757537 -1.44980589546683 -2.91542653252918,1.23143043581088 -1.46952355369781 -2.95767883503644,1.24356017692077 -1.48818737903916 -3.00132289466106,1.2527204915264 -1.50570777494116 -3.04614919609277,1.25886740511731 -1.52200063393958 -3.09194254861523,1.26197140915283 -1.53698774141742 -3.138483119139,1.2620176027192 -1.55059715107771 -3.18554748752036,1.25900576406208 -1.56276353032509 -3.23290971909991,1.25295035165112 -1.57342847389788 -3.28034244931235,1.24388043477135 -1.58254078424523 -3.32761797516041,1.23183955397484 -1.59005671730331 -3.37450934831362,1.21688551206218 -1.5959401924907 -3.42079146458413,1.19909009659756 -1.60016296591487 -3.46624214454969,1.17853873528923 -1.6027047659583 -3.51064320013621,1.15533008588991 -1.60355339059327 -3.55378148203954,1.12957556258567 -1.6027047659583 -3.59544990295855,1.10139880114707 -1.60016296591487 -3.63544843172717,1.07093506540992 -1.5959401924907 -3.67358505357321,1.03833059793508 -1.59005671730331 -3.70967669189409,1.00374191796432 -1.58254078424523 -3.74355008712442,0.967335070042516 -1.57342847389788 -3.77504262847653,0.929284826913179 -1.56276353032509 -3.80400313456104,0.889773850513921 -1.55059715107771 -3.83029257913998,0.848991815099414 -1.53698774141741 -3.85378475852869,0.807134496701444 -1.52200063393958 -3.87436689744237,0.764402833297081 -1.50570777494116 -3.891940190379,0.72100196019671 -1.48818737903916 -3.90642027593972,0.677140225282558 -1.46952355369781 -3.91773764180954,0.633028188825132 -1.44980589546683 -3.92583795845446,0.588877612678977 -1.42912905986906 -3.93068233993284,0.544900443710183 -1.40759230700227 -3.93224753056917,0.501307796335731 -1.38529902503655 -3.93052601659399,0.458308939059047 -1.36235623389463 -3.92552606221412,0.416110289866958 -1.33887407149783 -3.91727166993992,0.374914425310669 -1.31496526504393 -3.90580246536015,0.334919108027743 -1.29074458985506 -3.89117350691752,0.296316337373485 -1.26632831839353 -3.87345502159809,0.259291427719229 -1.24183366209053 -3.85273206780345,0.224022118842197 -1.21737820866727 -3.82910412702388,0.190677722677547 -1.19307935764975 -3.80268462627299,0.159418310528662 -1.16905375678694 -3.77360039357605,0.13039394463751 -1.14541674207787 -3.7419910491263,0.103743957803975 -1.12228178409597 -3.70800833503176,0.079596284512354 -1.09975994326827 -3.67181538687033,0.058066846776 -1.07795933672482 -3.63358595054991,0.039258997648375 -1.05698461927736 -3.59350354823325,0.023263025071983 -1.03693648101917 -3.55176059733134,0.010155718446952 -1.01791116395762 -3.50855748679486,0.0 -1.0 -3.46410161513776)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (0.5 -1.0 -2.59807621135332,0.543577773425908 -1.01791116395762 -2.58464338569351,0.587546417985528 -1.03693648101917 -2.57439309093773,0.631694860183701 -1.05698461927736 -2.56737453409618,0.675811163398452 -1.07795933672482 -2.56362140807291,0.719683545292469 -1.09975994326827 -2.56315172992158,0.763101394483934 -1.12228178409597 -2.5659677543537,0.805856281596113 -1.14541674207787 -2.57205596291479,0.847742959832148 -1.16905375678694 -2.58138712888039,0.888560350271717 -1.19307935764976 -2.59391645756022,0.928112507159618 -1.21737820866727 -2.60958380133721,0.96620955855235 -1.24183366209053 -2.62831394840886,1.00266861780709 -1.26632831839353 -2.65001698384495,1.03731466153743 -1.29074458985506 -2.67458872122829,1.06998136982112 -1.31496526504393 -2.7019112028064,1.10051192462654 -1.33887407149783 -2.73185326575312,1.12875976262485 -1.36235623389463 -2.76427117182176,1.15458927877392 -1.38529902503655 -2.79900929736714,1.17787647729662 -1.40759230700227 -2.83590088042404,1.19850956692819 -1.42912905986906 -2.8747688212557,1.21638949757537 -1.44980589546683 -2.91542653252918,1.23143043581088 -1.46952355369781 -2.95767883503644,1.24356017692077 -1.48818737903916 -3.00132289466106,1.2527204915264 -1.50570777494116 -3.04614919609277,1.25886740511731 -1.52200063393958 -3.09194254861523,1.26197140915283 -1.53698774141742 -3.138483119139,1.2620176027192 -1.55059715107771 -3.18554748752036,1.25900576406208 -1.56276353032509 -3.23290971909991,1.25295035165112 -1.57342847389788 -3.28034244931235,1.24388043477135 -1.58254078424523 -3.32761797516041,1.23183955397484 -1.59005671730331 -3.37450934831362,1.21688551206218 -1.5959401924907 -3.42079146458413,1.19909009659756 -1.60016296591487 -3.46624214454969,1.17853873528923 -1.6027047659583 -3.51064320013621,1.15533008588991 -1.60355339059327 -3.55378148203954,1.12957556258567 -1.6027047659583 -3.59544990295855,1.10139880114707 -1.60016296591487 -3.63544843172717,1.07093506540992 -1.5959401924907 -3.67358505357321,1.03833059793508 -1.59005671730331 -3.70967669189409,1.00374191796432 -1.58254078424523 -3.74355008712442,0.967335070042516 -1.57342847389788 -3.77504262847653,0.929284826913179 -1.56276353032509 -3.80400313456104,0.889773850513921 -1.55059715107771 -3.83029257913998,0.848991815099414 -1.53698774141741 -3.85378475852869,0.807134496701444 -1.52200063393958 -3.87436689744237,0.764402833297081 -1.50570777494116 -3.891940190379,0.72100196019671 -1.48818737903916 -3.90642027593972,0.677140225282558 -1.46952355369781 -3.91773764180954,0.633028188825132 -1.44980589546683 -3.92583795845446,0.588877612678977 -1.42912905986906 -3.93068233993284,0.544900443710183 -1.40759230700227 -3.93224753056917,0.501307796335731 -1.38529902503655 -3.93052601659399,0.458308939059047 -1.36235623389463 -3.92552606221412,0.416110289866958 -1.33887407149783 -3.91727166993992,0.374914425310669 -1.31496526504393 -3.90580246536015,0.334919108027743 -1.29074458985506 -3.89117350691752,0.296316337373485 -1.26632831839353 -3.87345502159809,0.259291427719229 -1.24183366209053 -3.85273206780345,0.224022118842197 -1.21737820866727 -3.82910412702388,0.190677722677547 -1.19307935764975 -3.80268462627299,0.159418310528662 -1.16905375678694 -3.77360039357605,0.13039394463751 -1.14541674207787 -3.7419910491263,0.103743957803975 -1.12228178409597 -3.70800833503176,0.079596284512354 -1.09975994326827 -3.67181538687033,0.058066846776 -1.07795933672482 -3.63358595054991,0.039258997648375 -1.05698461927736 -3.59350354823325,0.023263025071983 -1.03693648101917 -3.55176059733134,0.010155718446952 -1.01791116395762 -3.50855748679486,0.0 -1.0 -3.46410161513776)",
    )

    # OGRFeature(entities):45
    #   EntityHandle (String) = 1EE
    #   POLYGON Z ((1 -1 -1.73205080756888,1.75 -1.5 -2.1650635094611,1.25 -1.5 -3.03108891324553,0.5 -1.0 -2.59807621135332,1 -1 -1.73205080756888))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((1 -1 -1.73205080756888,1.75 -1.5 -2.1650635094611,1.25 -1.5 -3.03108891324553,0.5 -1.0 -2.59807621135332,1 -1 -1.73205080756888))",
    )

    # OGRFeature(entities):46
    #   EntityHandle (String) = 1EF
    #   POLYGON Z ((1.5 -2.0 -4.33012701892219,1.0 -2.0 -5.19615242270663,0.25 -1.5 -4.76313972081441,0.75 -1.5 -3.89711431702997,1.5 -2.0 -4.33012701892219))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((1.5 -2.0 -4.33012701892219,1.0 -2.0 -5.19615242270663,0.25 -1.5 -4.76313972081441,0.75 -1.5 -3.89711431702997,1.5 -2.0 -4.33012701892219))",
    )

    # OGRFeature(entities):47
    #   EntityHandle (String) = 1F1
    #   POLYGON Z ((2.0 -4.0 -10.3923048454133,1.5 -4.0 -11.2583302491977,2.25 -4.5 -11.6913429510899,2.75 -4.5 -10.8253175473055,2.0 -4.0 -10.3923048454133))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((2.0 -4.0 -10.3923048454133,1.5 -4.0 -11.2583302491977,2.25 -4.5 -11.6913429510899,2.75 -4.5 -10.8253175473055,2.0 -4.0 -10.3923048454133))",
    )

    # OGRFeature(entities):48
    #   EntityHandle (String) = 1F2
    #   LINESTRING Z (0.5 -1.0 -2.59807621135332,0.53846153846154 -1.07692307692308 -2.79792822761126,0.576923076923078 -1.15384615384615 -2.99778024386921,0.615384615384617 -1.23076923076923 -3.19763226012716,0.653846153846155 -1.30769230769231 -3.3974842763851,0.692307692307694 -1.38461538461538 -3.59733629264305,0.730769230769232 -1.46153846153846 -3.797188308901,0.769230769230771 -1.53846153846154 -3.99704032515895,0.807692307692309 -1.61538461538462 -4.19689234141689,0.846153846153848 -1.69230769230769 -4.39674435767484,0.884615384615386 -1.76923076923077 -4.59659637393279,0.923076923076924 -1.84615384615385 -4.79644839019073,0.961538461538463 -1.92307692307692 -4.99630040644868,1.0 -2 -5.19615242270663,1.03846153846154 -2.07692307692308 -5.39600443896458,1.07692307692308 -2.15384615384615 -5.59585645522253,1.11538461538462 -2.23076923076923 -5.79570847148047,1.15384615384616 -2.30769230769231 -5.99556048773842,1.19230769230769 -2.38461538461538 -6.19541250399637,1.23076923076923 -2.46153846153846 -6.39526452025432,1.26923076923077 -2.53846153846154 -6.59511653651226,1.30769230769231 -2.61538461538461 -6.79496855277021,1.34615384615385 -2.69230769230769 -6.99482056902816,1.38461538461539 -2.76923076923077 -7.19467258528611,1.42307692307693 -2.84615384615385 -7.39452460154405,1.46153846153846 -2.92307692307692 -7.594376617802,1.5 -3 -7.79422863405995,1.53846153846154 -3.07692307692308 -7.99408065031789,1.57692307692308 -3.15384615384615 -8.19393266657584,1.61538461538462 -3.23076923076923 -8.39378468283379,1.65384615384616 -3.30769230769231 -8.59363669909174,1.6923076923077 -3.38461538461539 -8.79348871534969,1.73076923076923 -3.46153846153846 -8.99334073160763,1.76923076923077 -3.53846153846154 -9.19319274786558,1.80769230769231 -3.61538461538461 -9.39304476412353,1.84615384615385 -3.69230769230769 -9.59289678038147,1.88461538461539 -3.76923076923077 -9.79274879663942,1.92307692307693 -3.84615384615385 -9.99260081289737,1.96153846153847 -3.92307692307692 -10.1924528291553,2.0 -4 -10.3923048454133)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (0.5 -1.0 -2.59807621135332,0.53846153846154 -1.07692307692308 -2.79792822761126,0.576923076923078 -1.15384615384615 -2.99778024386921,0.615384615384617 -1.23076923076923 -3.19763226012716,0.653846153846155 -1.30769230769231 -3.3974842763851,0.692307692307694 -1.38461538461538 -3.59733629264305,0.730769230769232 -1.46153846153846 -3.797188308901,0.769230769230771 -1.53846153846154 -3.99704032515895,0.807692307692309 -1.61538461538462 -4.19689234141689,0.846153846153848 -1.69230769230769 -4.39674435767484,0.884615384615386 -1.76923076923077 -4.59659637393279,0.923076923076924 -1.84615384615385 -4.79644839019073,0.961538461538463 -1.92307692307692 -4.99630040644868,1.0 -2 -5.19615242270663,1.03846153846154 -2.07692307692308 -5.39600443896458,1.07692307692308 -2.15384615384615 -5.59585645522253,1.11538461538462 -2.23076923076923 -5.79570847148047,1.15384615384616 -2.30769230769231 -5.99556048773842,1.19230769230769 -2.38461538461538 -6.19541250399637,1.23076923076923 -2.46153846153846 -6.39526452025432,1.26923076923077 -2.53846153846154 -6.59511653651226,1.30769230769231 -2.61538461538461 -6.79496855277021,1.34615384615385 -2.69230769230769 -6.99482056902816,1.38461538461539 -2.76923076923077 -7.19467258528611,1.42307692307693 -2.84615384615385 -7.39452460154405,1.46153846153846 -2.92307692307692 -7.594376617802,1.5 -3 -7.79422863405995,1.53846153846154 -3.07692307692308 -7.99408065031789,1.57692307692308 -3.15384615384615 -8.19393266657584,1.61538461538462 -3.23076923076923 -8.39378468283379,1.65384615384616 -3.30769230769231 -8.59363669909174,1.6923076923077 -3.38461538461539 -8.79348871534969,1.73076923076923 -3.46153846153846 -8.99334073160763,1.76923076923077 -3.53846153846154 -9.19319274786558,1.80769230769231 -3.61538461538461 -9.39304476412353,1.84615384615385 -3.69230769230769 -9.59289678038147,1.88461538461539 -3.76923076923077 -9.79274879663942,1.92307692307693 -3.84615384615385 -9.99260081289737,1.96153846153847 -3.92307692307692 -10.1924528291553,2.0 -4 -10.3923048454133)",
    )

    # OGRFeature(entities):49
    #   EntityHandle (String) = 1F3
    #   LINESTRING Z (-3.25 -0.5 -7.36121593216773,-3.07367580370539 -0.493674033614862 -7.03389967520965,-2.89709873733542 -0.487853807380434 -6.70789736970626,-2.72001593081474 -0.483045061447428 -6.38452296711225,-2.542174514068 -0.479753535966554 -6.06509041888229,-2.36332161701984 -0.478484971088521 -5.75091367647109,-2.1832043695949 -0.479745106964042 -5.44330669133333,-2.00156990171783 -0.484039683743825 -5.14358341492368,-1.81816534331327 -0.491874441578583 -4.85305779869685,-1.63273782430587 -0.503755120619026 -4.57304379410752,-1.44503447462027 -0.520187461015863 -4.30485535261037,-1.25480242418112 -0.541677202919806 -4.0498064256601,-1.06178880291306 -0.568730086481565 -3.80921096471139,-0.865740740740739 -0.601851851851852 -3.58438292121893,-0.666405367588798 -0.641548239181375 -3.3766362466374,-0.463529813381883 -0.688324988620846 -3.1872848924215,-0.256861208044639 -0.742687840320976 -3.0176428100259,-0.04614668150171 -0.805142534432475 -2.86902395090531,0.168866636322258 -0.876194811106053 -2.74274226651439,0.388431615502622 -0.956350410492422 -2.64011170830786,0.612790589861596 -1.04609400023601 -2.5623914797631,0.841943559399181 -1.14542558033682 -2.50958158088012,1.07564819029316 -1.25386048315042 -2.48042280818152,1.31365161246818 -1.3708929685261 -2.4736012102126,1.55570095584888 -1.49601729631315 -2.48780283551868,1.80154335035992 -1.62872772636086 -2.52171373264507,2.05092592592592 -1.76851851851852 -2.57401995013708,2.30359581247155 -1.91488393263541 -2.64340753654003,2.55930013992144 -2.06731822856083 -2.72856254039923,2.81778603820024 -2.22531566614407 -2.82817101025998,3.07880063723259 -2.38837050523441 -2.94091899466761,3.34209106694314 -2.55597700568115 -3.06549254216743,3.60740445725653 -2.72762942733357 -3.20057770130475,3.87448793809741 -2.90282203004097 -3.34486052062488,4.14308863939042 -3.08104907365262 -3.49702704867313,4.4129536910602 -3.26180481801784 -3.65576333399482,4.68383022303141 -3.44458352298589 -3.81975542513526,4.95546536522868 -3.62887944840608 -3.98768937063976,5.22760624757667 -3.81418685412768 -4.15825121905363,5.5 -4 -4.33012701892219)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (-3.25 -0.5 -7.36121593216773,-3.07367580370539 -0.493674033614862 -7.03389967520965,-2.89709873733542 -0.487853807380434 -6.70789736970626,-2.72001593081474 -0.483045061447428 -6.38452296711225,-2.542174514068 -0.479753535966554 -6.06509041888229,-2.36332161701984 -0.478484971088521 -5.75091367647109,-2.1832043695949 -0.479745106964042 -5.44330669133333,-2.00156990171783 -0.484039683743825 -5.14358341492368,-1.81816534331327 -0.491874441578583 -4.85305779869685,-1.63273782430587 -0.503755120619026 -4.57304379410752,-1.44503447462027 -0.520187461015863 -4.30485535261037,-1.25480242418112 -0.541677202919806 -4.0498064256601,-1.06178880291306 -0.568730086481565 -3.80921096471139,-0.865740740740739 -0.601851851851852 -3.58438292121893,-0.666405367588798 -0.641548239181375 -3.3766362466374,-0.463529813381883 -0.688324988620846 -3.1872848924215,-0.256861208044639 -0.742687840320976 -3.0176428100259,-0.04614668150171 -0.805142534432475 -2.86902395090531,0.168866636322258 -0.876194811106053 -2.74274226651439,0.388431615502622 -0.956350410492422 -2.64011170830786,0.612790589861596 -1.04609400023601 -2.5623914797631,0.841943559399181 -1.14542558033682 -2.50958158088012,1.07564819029316 -1.25386048315042 -2.48042280818152,1.31365161246818 -1.3708929685261 -2.4736012102126,1.55570095584888 -1.49601729631315 -2.48780283551868,1.80154335035992 -1.62872772636086 -2.52171373264507,2.05092592592592 -1.76851851851852 -2.57401995013708,2.30359581247155 -1.91488393263541 -2.64340753654003,2.55930013992144 -2.06731822856083 -2.72856254039923,2.81778603820024 -2.22531566614407 -2.82817101025998,3.07880063723259 -2.38837050523441 -2.94091899466761,3.34209106694314 -2.55597700568115 -3.06549254216743,3.60740445725653 -2.72762942733357 -3.20057770130475,3.87448793809741 -2.90282203004097 -3.34486052062488,4.14308863939042 -3.08104907365262 -3.49702704867313,4.4129536910602 -3.26180481801784 -3.65576333399482,4.68383022303141 -3.44458352298589 -3.81975542513526,4.95546536522868 -3.62887944840608 -3.98768937063976,5.22760624757667 -3.81418685412768 -4.15825121905363,5.5 -4 -4.33012701892219)",
    )

    # OGRFeature(entities):50
    #   EntityHandle (String) = 1F4
    #   POINT Z (1.75 -3.5 -9.09326673973661)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (1.75 -3.5 -9.09326673973661)")

    # OGRFeature(entities):51
    #   EntityHandle (String) = 1F5
    #   POINT Z (5.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (5.5 1.0 -0.866025403784439)")

    # OGRFeature(entities):52
    #   EntityHandle (String) = 1F6
    #   LINESTRING Z (0 0 0,1.375 0.25 -0.21650635094611)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (0 0 0,1.375 0.25 -0.21650635094611)"
    )

    # OGRFeature(entities):53
    #   EntityHandle (String) = 1F7
    #   LINESTRING Z (1.375 0.25 -0.21650635094611,2.0 1.0 -9.68245836551854e-17,2.125 -0.25 -0.649519052838329,1.375 0.25 -0.21650635094611)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (1.375 0.25 -0.21650635094611,2.0 1.0 -9.68245836551854e-17,2.125 -0.25 -0.649519052838329,1.375 0.25 -0.21650635094611)",
    )

    # OGRFeature(entities):54
    #   EntityHandle (String) = 1F8
    #   LINESTRING Z (1.375 0.25 -0.21650635094611,2.125 -0.25 -0.649519052838329,2.75 0.5 -0.43301270189222,1.375 0.25 -0.21650635094611)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (1.375 0.25 -0.21650635094611,2.125 -0.25 -0.649519052838329,2.75 0.5 -0.43301270189222,1.375 0.25 -0.21650635094611)",
    )

    # OGRFeature(entities):55
    #   EntityHandle (String) = 1FD
    #   LINESTRING Z (3.71114561800017 0.105572809000085 -0.866025403784439,3.77009625337411 -0.001434563330173 -0.933566819059727,3.83747480591229 -0.103563047131737 -1.00077917934903,3.91295301407961 -0.200315082697444 -1.06733503278919,3.99616315563294 -0.2912193038355 -1.13291012595241,4.08669983912687 -0.37583283431818 -1.19718498357639,4.18412197894014 -0.453743445530294 -1.25984646501566,4.28795494420042 -0.524571564805566 -1.32058928983137,4.39769287113821 -0.587972124666501 -1.37911752508686,4.51280112760429 -0.643636243958462 -1.43514602710321,4.6327189177438 -0.691292732687594 -1.48840183065055,4.75686201413727 -0.730709413231224 -1.53862547880726,4.88462560409789 -0.761694251483893 -1.58557228700798,5.01538723625813 -0.784096292428219 -1.62901353512224,5.14850985309022 -0.797806395572568 -1.66873758175591,5.28334489458639 -0.802757766672552 -1.70455089534686,5.41923545797803 -0.798926283145887 -1.7362789970312,5.55551949809999 -0.786330611595171 -1.76376731068683,5.691533052808 -0.76503211686609 -1.78688191601273,5.82661347773556 -0.735134563084044 -1.80551020097523,5.96010267463067 -0.69678360812577 -1.8195614104425,6.09135029754434 -0.650166093988798 -1.82896708833443,6.2197169212507 -0.595509136516029 -1.83368141113379,6.34457715646241 -0.533079018910172 -1.83368141113379,6.46532269666444 -0.463179894428748 -1.82896708833443,6.58136528172222 -0.386152304579996 -1.8195614104425,6.69213956382591 -0.302371520038872 -1.80551020097523,6.79710586180808 -0.212245712366048 -1.78688191601273,6.89575279041615 -0.116213965437088 -1.76376731068683,6.9875997517299 -0.014744136269953 -1.7362789970312,7.07219927658622 0.091669424327363 -1.70455089534686,7.14913920460393 0.202508280184287 -1.66873758175591,7.2180446921877 0.317232435536567 -1.62901353512224,7.27858003872839 0.435282965831356 -1.58557228700798,7.33045032210263 0.556084740751457 -1.53862547880726,7.37340283550381 0.679049226192411 -1.48840183065055,7.4072283186042 0.803577351541497 -1.43514602710321,7.43176197705028 0.929062428289537 -1.37911752508686,7.44688428532421 1.05489310575633 -1.32058928983137,7.45252156906016 1.18045634952971 -1.25984646501566,7.44864636397843 1.3051404281076 -1.19718498357639,7.43527754968864 1.42833789319235 -1.13291012595241,7.41248025771019 1.54944853911785 -1.06733503278919,7.38036555415802 1.66788232699113 -1.00077917934903,7.33908989863968 1.78306225930261 -0.933566819059728,7.28885438199984 1.89442719099992 -0.86602540378444,
    # 7.2299037466259 2.00143456333018 -0.798483988509152,7.16252519408772 2.10356304713174 -0.731271628219845,7.08704698592039 2.20031508269745 -0.664715774779688,7.00383684436707 2.2912193038355 -0.599140681616468,6.91330016087314 2.37583283431818 -0.534865823992492,6.81587802105987 2.4537434455303 -0.472204342553219,6.71204505579959 2.52457156480557 -0.411461517737508,6.6023071288618 2.5879721246665 -0.352933282482017,6.48719887239572 2.64363624395846 -0.296904780465671,6.36728108225621 2.6912927326876 -0.24364897691833,6.24313798586274 2.73070941323123 -0.19342532876162,6.11537439590212 2.7616942514839 -0.146478520560899,5.98461276374187 2.78409629242822 -0.103037272446642,5.85149014690979 2.79780639557257 -0.0633132258129652,5.71665510541362 2.80275776667256 -0.0274999122220189,5.58076454202198 2.79892628314589 0.00422818946232298,5.44448050190002 2.78633061159517 0.0317165031179519,5.30846694719201 2.76503211686609 0.0548311084438533,5.17338652226444 2.73513456308405 0.0734593934063514,5.03989732536933 2.69678360812577 0.0875106028736174,4.90864970245567 2.6501660939888 0.0969162807655497,4.78028307874931 2.59550913651603 0.101630603564914,4.6554228435376 2.53307901891017 0.101630603564914,4.53467730333557 2.46317989442875 0.0969162807655497,4.41863471827779 2.38615230458 0.0875106028736175,4.3078604361741 2.30237152003888 0.0734593934063515,4.20289413819193 2.21224571236605 0.0548311084438534,4.10424720958385 2.11621396543709 0.031716503117952,4.01240024827011 2.01474413626996 0.00422818946232319,3.92780072341379 1.90833057567264 -0.0274999122220187,3.85086079539608 1.79749171981572 -0.0633132258129645,3.7819553078123 1.68276756446344 -0.103037272446642,3.72141996127162 1.56471703416865 -0.146478520560898,3.66954967789738 1.44391525924855 -0.19342532876162,3.6265971644962 1.32095077380759 -0.243648976918329,3.5927716813958 1.19642264845851 -0.296904780465671,3.56823802294973 1.07093757171047 -0.352933282482016,3.5531157146758 0.945106894243672 -0.411461517737508,3.54747843093985 0.819543650470287 -0.472204342553219,3.55135363602158 0.694859571892403 -0.534865823992493,3.56472245031136 0.571662106807651 -0.599140681616467,3.58751974228981 0.450551460882157 -0.664715774779687,3.61963444584198 0.332117673008871 -0.731271628219845,3.66091010136033 0.216937740697387 -0.798483988509152,3.71114561800017 0.105572809000085 -0.866025403784439)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (3.71114561800017 0.105572809000085 -0.866025403784439,3.77009625337411 -0.001434563330173 -0.933566819059727,3.83747480591229 -0.103563047131737 -1.00077917934903,3.91295301407961 -0.200315082697444 -1.06733503278919,3.99616315563294 -0.2912193038355 -1.13291012595241,4.08669983912687 -0.37583283431818 -1.19718498357639,4.18412197894014 -0.453743445530294 -1.25984646501566,4.28795494420042 -0.524571564805566 -1.32058928983137,4.39769287113821 -0.587972124666501 -1.37911752508686,4.51280112760429 -0.643636243958462 -1.43514602710321,4.6327189177438 -0.691292732687594 -1.48840183065055,4.75686201413727 -0.730709413231224 -1.53862547880726,4.88462560409789 -0.761694251483893 -1.58557228700798,5.01538723625813 -0.784096292428219 -1.62901353512224,5.14850985309022 -0.797806395572568 -1.66873758175591,5.28334489458639 -0.802757766672552 -1.70455089534686,5.41923545797803 -0.798926283145887 -1.7362789970312,5.55551949809999 -0.786330611595171 -1.76376731068683,5.691533052808 -0.76503211686609 -1.78688191601273,5.82661347773556 -0.735134563084044 -1.80551020097523,5.96010267463067 -0.69678360812577 -1.8195614104425,6.09135029754434 -0.650166093988798 -1.82896708833443,6.2197169212507 -0.595509136516029 -1.83368141113379,6.34457715646241 -0.533079018910172 -1.83368141113379,6.46532269666444 -0.463179894428748 -1.82896708833443,6.58136528172222 -0.386152304579996 -1.8195614104425,6.69213956382591 -0.302371520038872 -1.80551020097523,6.79710586180808 -0.212245712366048 -1.78688191601273,6.89575279041615 -0.116213965437088 -1.76376731068683,6.9875997517299 -0.014744136269953 -1.7362789970312,7.07219927658622 0.091669424327363 -1.70455089534686,7.14913920460393 0.202508280184287 -1.66873758175591,7.2180446921877 0.317232435536567 -1.62901353512224,7.27858003872839 0.435282965831356 -1.58557228700798,7.33045032210263 0.556084740751457 -1.53862547880726,7.37340283550381 0.679049226192411 -1.48840183065055,7.4072283186042 0.803577351541497 -1.43514602710321,7.43176197705028 0.929062428289537 -1.37911752508686,7.44688428532421 1.05489310575633 -1.32058928983137,7.45252156906016 1.18045634952971 -1.25984646501566,7.44864636397843 1.3051404281076 -1.19718498357639,7.43527754968864 1.42833789319235 -1.13291012595241,7.41248025771019 1.54944853911785 -1.06733503278919,7.38036555415802 1.66788232699113 -1.00077917934903,7.33908989863968 1.78306225930261 -0.933566819059728,7.28885438199984 1.89442719099992 -0.86602540378444,7.2299037466259 2.00143456333018 -0.798483988509152,7.16252519408772 2.10356304713174 -0.731271628219845,7.08704698592039 2.20031508269745 -0.664715774779688,"
        + "7.00383684436707 2.2912193038355 -0.599140681616468,6.91330016087314 2.37583283431818 -0.534865823992492,6.81587802105987 2.4537434455303 -0.472204342553219,6.71204505579959 2.52457156480557 -0.411461517737508,6.6023071288618 2.5879721246665 -0.352933282482017,6.48719887239572 2.64363624395846 -0.296904780465671,6.36728108225621 2.6912927326876 -0.24364897691833,6.24313798586274 2.73070941323123 -0.19342532876162,6.11537439590212 2.7616942514839 -0.146478520560899,5.98461276374187 2.78409629242822 -0.103037272446642,5.85149014690979 2.79780639557257 -0.0633132258129652,5.71665510541362 2.80275776667256 -0.0274999122220189,5.58076454202198 2.79892628314589 0.00422818946232298,5.44448050190002 2.78633061159517 0.0317165031179519,5.30846694719201 2.76503211686609 0.0548311084438533,5.17338652226444 2.73513456308405 0.0734593934063514,5.03989732536933 2.69678360812577 0.0875106028736174,4.90864970245567 2.6501660939888 0.0969162807655497,4.78028307874931 2.59550913651603 0.101630603564914,4.6554228435376 2.53307901891017 0.101630603564914,4.53467730333557 2.46317989442875 0.0969162807655497,4.41863471827779 2.38615230458 0.0875106028736175,4.3078604361741 2.30237152003888 0.0734593934063515,4.20289413819193 2.21224571236605 0.0548311084438534,4.10424720958385 2.11621396543709 0.031716503117952,4.01240024827011 2.01474413626996 0.00422818946232319,3.92780072341379 1.90833057567264 -0.0274999122220187,3.85086079539608 1.79749171981572 -0.0633132258129645,3.7819553078123 1.68276756446344 -0.103037272446642,3.72141996127162 1.56471703416865 -0.146478520560898,3.66954967789738 1.44391525924855 -0.19342532876162,3.6265971644962 1.32095077380759 -0.243648976918329,3.5927716813958 1.19642264845851 -0.296904780465671,3.56823802294973 1.07093757171047 -0.352933282482016,3.5531157146758 0.945106894243672 -0.411461517737508,3.54747843093985 0.819543650470287 -0.472204342553219,3.55135363602158 0.694859571892403 -0.534865823992493,3.56472245031136 0.571662106807651 -0.599140681616467,3.58751974228981 0.450551460882157 -0.664715774779687,3.61963444584198 0.332117673008871 -0.731271628219845,3.66091010136033 0.216937740697387 -0.798483988509152,3.71114561800017 0.105572809000085 -0.866025403784439)",
    )

    # OGRFeature(entities):56
    #   EntityHandle (String) = 1FE
    #   LINESTRING Z (4.25 -0.5 -1.29903810567666,5.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (4.25 -0.5 -1.29903810567666,5.5 1.0 -0.866025403784439)"
    )

    # OGRFeature(entities):57
    #   EntityHandle (String) = 1FF
    #   LINESTRING Z (5.5 1.0 -0.866025403784439,6.75 2.5 -0.43301270189222)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (5.5 1.0 -0.866025403784439,6.75 2.5 -0.43301270189222)"
    )

    # OGRFeature(entities):58
    #   EntityHandle (String) = 200
    #   LINESTRING Z (4.75 1.5 -0.43301270189222,5.5 1.0 -0.866025403784439)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (4.75 1.5 -0.43301270189222,5.5 1.0 -0.866025403784439)"
    )

    # OGRFeature(entities):59
    #   EntityHandle (String) = 201
    #   LINESTRING Z (5.5 1.0 -0.866025403784439,6.25 0.5 -1.29903810567666)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat, "LINESTRING Z (5.5 1.0 -0.866025403784439,6.25 0.5 -1.29903810567666)"
    )

    # OGRFeature(entities):60
    #   EntityHandle (String) = 202
    #   LINESTRING Z (4.25 -0.5 -1.29903810567666,4.30536229248331 -0.531224312261799 -1.32818874766841,4.36654473979309 -0.554988653592388 -1.35508777630525,4.43324926719606 -0.571177246509588 -1.37960414222355,4.50515089693985 -0.579711221815978 -1.40161840415293,4.58189933151187 -0.580549002841697 -1.42102331082214,4.6631206602536 -0.573686508001801 -1.43772432347645,4.74841918101576 -0.559157170681335 -1.451640076461,4.83737932797937 -0.53703177635124 -1.46270277362604,4.92956769625067 -0.507418117708657 -1.47085851862302,5.02453515336618 -0.470460469521736 -1.47606757748212,5.12181902742094 -0.426338885737475 -1.47830457219218,5.22094536115948 -0.375268322276984 -1.47755860433986,5.32143122104797 -0.317497589791849 -1.47383330820552,5.42278705007785 -0.253308141483641 -1.46714683305749,5.52451905283833 -0.183012701892219 -1.45753175473055,5.62613160123803 -0.106953743333199 -1.44503491691981,5.7271296491552 -0.025501817407262 -1.42971720296291,5.82702114425268 0.060946250290002 -1.41165323922251,5.92531942520741 0.151969293462501 -1.39093103151418,6.02154559267549 0.247123856993501 -1.36765153635088,6.11523084244164 0.345946357414384 -1.34192816909299,6.2059187493862 0.4479553414367 -1.31388625140011,6.29316749114245 0.552653831544204 -1.28366240067645,6.37655200061077 0.659531747217344 -1.25140386448466,6.45566603684282 0.768068389994292 -1.21726780317044,6.53012416420658 0.877734980261504 -1.18142052419318,6.59956363019005 0.987997233414845 -1.1440366718927,6.66364613269498 1.09831796284048 -1.10529837663958,6.72205946821072 1.20815969703403 -1.06539436751435,6.77451905283833 1.31698729810778 -1.02451905283833,6.82076930875472 1.4242705689286 -0.982871573035841,6.86058490936212 1.52948683618513 -0.940654830442097,6.89377187705662 1.63212349679959 -0.898074500783399,6.92016852826756 1.73168051527848 -0.855338031145687,6.93964626116363 1.8276728598352 -0.81265362931318,6.95211018218804 1.91963286541618 -0.770229249400985,6.95749956837044 2.00711251211804 -0.72827157872355,6.95578816316308 2.08968560789544 -0.686985030834846,6.9469843043601 2.166949864926 -0.646570749646076,6.93113088347664 2.23852885951603 -0.607225629472764,6.90830513678565 2.30407386599898 -0.569141355785437,6.87861826903058 2.36326555569183 -0.53250347133728,6.84221491164701 2.41581555263232 -0.497490472218499,6.79927241813288 2.46146783851767 -0.464272938241304,6.75 2.5 -0.43301270189222,6.69463770751669 2.5312243122618 -0.40386205990047,6.63345526020692 2.55498865359239 -0.376963031263626,
    # 6.56675073280394 2.57117724650959 -0.352446665345325,6.49484910306015 2.57971122181598 -0.330432403415945,6.41810066848813 2.5805490028417 -0.311027496746741,6.3368793397464 2.5736865080018 -0.294326484092429,6.25158081898424 2.55915717068134 -0.280410731107883,6.16262067202063 2.53703177635124 -0.269348033942836,6.07043230374933 2.50741811770866 -0.261192288945857,5.97546484663382 2.47046046952174 -0.255983230086761,5.87818097257907 2.42633888573748 -0.253746235376694,5.77905463884053 2.37526832227698 -0.254492203229023,5.67856877895203 2.31749758979185 -0.258217499363356,5.57721294992215 2.25330814148364 -0.264903974511391,5.47548094716167 2.18301270189222 -0.27451905283833,5.37386839876197 2.1069537433332 -0.287015890649068,5.2728703508448 2.02550181740726 -0.302333604605968,5.17297885574732 1.93905374971 -0.320397568346365,5.07468057479259 1.8480307065375 -0.341119776054697,4.97845440732451 1.7528761430065 -0.364399271218,4.88476915755836 1.65405364258562 -0.390122638475884,4.79408125061381 1.5520446585633 -0.418164556168773,4.70683250885755 1.4473461684558 -0.44838840689243,4.62344799938923 1.34046825278266 -0.48064694308422,4.54433396315718 1.23193161000571 -0.514783004398435,4.46987583579342 1.1222650197385 -0.550630283375696,4.40043636980995 1.01200276658516 -0.588014135676182,4.33635386730502 0.901682037159526 -0.626752430929296,4.27794053178928 0.791840302965968 -0.666656440054525,4.22548094716167 0.68301270189222 -0.707531754730549,4.17923069124529 0.575729431071401 -0.749179234533037,4.13941509063789 0.470513163814874 -0.791395977126782,4.10622812294338 0.36787650320041 -0.833976306785479,4.07983147173244 0.268319484721523 -0.876712776423191,4.06035373883638 0.172327140164803 -0.919397178255699,4.04788981781196 0.080367134583816 -0.961821558167894,4.04250043162957 -0.007112512118036 -1.00377922884533,4.04421183683692 -0.089685607895445 -1.04506577673403,4.0530156956399 -0.166949864926001 -1.0854800579228,4.06886911652336 -0.238528859516028 -1.12482517809611,4.09169486321435 -0.304073865998978 -1.16290945178344,4.12138173096942 -0.363265555691829 -1.1995473362316,4.15778508835299 -0.415815552632322 -1.23456033535038,4.20072758186713 -0.461467838517673 -1.26777786932757,4.25 -0.5 -1.29903810567666)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (4.25 -0.5 -1.29903810567666,4.30536229248331 -0.531224312261799 -1.32818874766841,4.36654473979309 -0.554988653592388 -1.35508777630525,4.43324926719606 -0.571177246509588 -1.37960414222355,4.50515089693985 -0.579711221815978 -1.40161840415293,4.58189933151187 -0.580549002841697 -1.42102331082214,4.6631206602536 -0.573686508001801 -1.43772432347645,4.74841918101576 -0.559157170681335 -1.451640076461,4.83737932797937 -0.53703177635124 -1.46270277362604,4.92956769625067 -0.507418117708657 -1.47085851862302,5.02453515336618 -0.470460469521736 -1.47606757748212,5.12181902742094 -0.426338885737475 -1.47830457219218,5.22094536115948 -0.375268322276984 -1.47755860433986,5.32143122104797 -0.317497589791849 -1.47383330820552,5.42278705007785 -0.253308141483641 -1.46714683305749,5.52451905283833 -0.183012701892219 -1.45753175473055,5.62613160123803 -0.106953743333199 -1.44503491691981,5.7271296491552 -0.025501817407262 -1.42971720296291,5.82702114425268 0.060946250290002 -1.41165323922251,5.92531942520741 0.151969293462501 -1.39093103151418,6.02154559267549 0.247123856993501 -1.36765153635088,6.11523084244164 0.345946357414384 -1.34192816909299,6.2059187493862 0.4479553414367 -1.31388625140011,6.29316749114245 0.552653831544204 -1.28366240067645,6.37655200061077 0.659531747217344 -1.25140386448466,6.45566603684282 0.768068389994292 -1.21726780317044,6.53012416420658 0.877734980261504 -1.18142052419318,6.59956363019005 0.987997233414845 -1.1440366718927,6.66364613269498 1.09831796284048 -1.10529837663958,6.72205946821072 1.20815969703403 -1.06539436751435,6.77451905283833 1.31698729810778 -1.02451905283833,6.82076930875472 1.4242705689286 -0.982871573035841,6.86058490936212 1.52948683618513 -0.940654830442097,6.89377187705662 1.63212349679959 -0.898074500783399,6.92016852826756 1.73168051527848 -0.855338031145687,6.93964626116363 1.8276728598352 -0.81265362931318,6.95211018218804 1.91963286541618 -0.770229249400985,6.95749956837044 2.00711251211804 -0.72827157872355,6.95578816316308 2.08968560789544 -0.686985030834846,6.9469843043601 2.166949864926 -0.646570749646076,6.93113088347664 2.23852885951603 -0.607225629472764,6.90830513678565 2.30407386599898 -0.569141355785437,6.87861826903058 2.36326555569183 -0.53250347133728,6.84221491164701 2.41581555263232 -0.497490472218499,6.79927241813288 2.46146783851767 -0.464272938241304,6.75 2.5 -0.43301270189222,6.69463770751669 2.5312243122618 -0.40386205990047,"
        + "6.63345526020692 2.55498865359239 -0.376963031263626,6.56675073280394 2.57117724650959 -0.352446665345325,6.49484910306015 2.57971122181598 -0.330432403415945,6.41810066848813 2.5805490028417 -0.311027496746741,6.3368793397464 2.5736865080018 -0.294326484092429,6.25158081898424 2.55915717068134 -0.280410731107883,6.16262067202063 2.53703177635124 -0.269348033942836,6.07043230374933 2.50741811770866 -0.261192288945857,5.97546484663382 2.47046046952174 -0.255983230086761,5.87818097257907 2.42633888573748 -0.253746235376694,5.77905463884053 2.37526832227698 -0.254492203229023,5.67856877895203 2.31749758979185 -0.258217499363356,5.57721294992215 2.25330814148364 -0.264903974511391,5.47548094716167 2.18301270189222 -0.27451905283833,5.37386839876197 2.1069537433332 -0.287015890649068,5.2728703508448 2.02550181740726 -0.302333604605968,5.17297885574732 1.93905374971 -0.320397568346365,5.07468057479259 1.8480307065375 -0.341119776054697,4.97845440732451 1.7528761430065 -0.364399271218,4.88476915755836 1.65405364258562 -0.390122638475884,4.79408125061381 1.5520446585633 -0.418164556168773,4.70683250885755 1.4473461684558 -0.44838840689243,4.62344799938923 1.34046825278266 -0.48064694308422,4.54433396315718 1.23193161000571 -0.514783004398435,4.46987583579342 1.1222650197385 -0.550630283375696,4.40043636980995 1.01200276658516 -0.588014135676182,4.33635386730502 0.901682037159526 -0.626752430929296,4.27794053178928 0.791840302965968 -0.666656440054525,4.22548094716167 0.68301270189222 -0.707531754730549,4.17923069124529 0.575729431071401 -0.749179234533037,4.13941509063789 0.470513163814874 -0.791395977126782,4.10622812294338 0.36787650320041 -0.833976306785479,4.07983147173244 0.268319484721523 -0.876712776423191,4.06035373883638 0.172327140164803 -0.919397178255699,4.04788981781196 0.080367134583816 -0.961821558167894,4.04250043162957 -0.007112512118036 -1.00377922884533,4.04421183683692 -0.089685607895445 -1.04506577673403,4.0530156956399 -0.166949864926001 -1.0854800579228,4.06886911652336 -0.238528859516028 -1.12482517809611,4.09169486321435 -0.304073865998978 -1.16290945178344,4.12138173096942 -0.363265555691829 -1.1995473362316,4.15778508835299 -0.415815552632322 -1.23456033535038,4.20072758186713 -0.461467838517673 -1.26777786932757,4.25 -0.5 -1.29903810567666)",
    )

    # OGRFeature(entities):61
    #   EntityHandle (String) = 203
    #   LINESTRING Z (2.75 0.5 -0.43301270189222,2.75597796157458 0.457022294808167 -0.455760312055479,2.76522760095779 0.414850974295674 -0.478918395607368,2.77770451483146 0.37368848382115 -0.502375781105475,2.79334880719819 0.333732425808347 -0.526019860294666,2.81208537691484 0.295174611146642 -0.549737128688177,2.83382427821898 0.258200138394064 -0.573413730451116,2.85846115251769 0.222986505203176 -0.596936004970611,2.88587772936573 0.189702756235451 -0.620191032488805,2.91594239422828 0.15850867165462 -0.643067176179329,2.94851082030252 0.129554000094665 -0.66545461806501,2.98342666136509 0.102977739784634 -0.687245886204105,3.02052230231928 0.078907471281269 -0.708336370614292,3.05961966383903 0.057458745012683 -0.728624825457685,3.10053105724686 0.038734526573237 -0.748013855076124,3.14306008552201 0.022824702432479 -0.766410381543502,3.18700258611332 0.009805648431019 -0.783726091490617,3.23214761103096 -0.00026013686519 -0.79987786005755,3.27827843951188 -0.007324332192091 -0.814788149938374,3.32517361839783 -0.011353025555964 -0.828385383602543,3.37260802523135 -0.012326877029509 -0.84060428690611,3.42035394896652 -0.010241211594056 -0.851386202443257,3.46818218310621 -0.0051060415822 -0.860679371133851,3.51586312601842 0.003053981386852 -0.868439180695292,3.56316788314946 0.014199684748517 -0.874628379805819,3.60986936584274 0.028277563042523 -0.879217256931186,3.65574338148826 0.045220034768334 -0.882183782956251,3.70056970976957 0.064945766813234 -0.883513716936746,3.74413315984144 0.087360064895672 -0.883200674463593,3.78622460336345 0.112355328149502 -0.881246158311542,3.82664197843012 0.139811565666877 -0.87765955122504,3.86519125957839 0.169596972520106 -0.872458070875934,3.90168738921576 0.201568562497258 -0.865666687209244,3.93595516599773 0.235572854514029 -0.857318002573801,3.96783008588991 0.271446609406727 -0.847452095213172,3.99715913187718 0.30901761356937 -0.836116326868217,4.02380150852887 0.348105505672996 -0.823365115414876,4.04762931789371 0.3885226424985 -0.809259673628662,4.06852817347981 0.430074999726512 -0.793867715329923,4.08639774937224 0.472563103360048 -0.777263130320539,4.10115226185219 0.515782987308575 -0.759525629672545,4.1127208812057 0.559527172536592 -0.740740363071464,4.12104807174486 0.603585663076254 -0.720997510051345,4.12609385840951 0.647746954122649 -0.700391847083764,4.1278340186693 0.691799047372318 -0.679022292599034,4.12626019880507 0.735530468730831 -0.656991432123733,4.12137995401128 0.778731283503886 -0.634405025814187,4.11321671212692 0.821194104198432 -0.61137150074998,4.10180966116915 0.862715086095846 -0.588001430424789,4.08721356120934 0.903094905817856 -0.564407003933257,4.06949848149492 0.942139718187561 -0.540701487402092,4.04874946407869 0.979662086792094 -0.516998680250836,4.02506611557057 1.01548188377972 -0.493412368892526,3.99856212897149 1.04942715457186 -0.470055780496802,3.96936473788492 1.08133494333891 -0.447041039437686,3.93761410572614 1.11105207527721 -0.424478629035376,3.9034626528613 1.13843589193168 -0.402476861176005,3.86707432490649 1.16335493603432 -0.381141356355458,3.8286238056993 1.1856895825708 -0.360574536643342,3.78829567872099 1.20533261304587 -0.340875134001137,3.74628354099508 1.22218973019068 -0.322137716314888,3.70278907371594 1.23618001064108 -0.304452233417734,3.65802107406893 1.247236293414 -0.287903585281599,3.61219445288998 1.25530550231681 -0.27257121445099,3.56552920297626 1.26034890074202 -0.258528724675415,3.51824934300062 1.26234227762414 -0.245843527571206,3.47058184209972 1.26127606366599 -0.234576519008966,3.42275553029828 1.2571553772766 -0.224781786780142,3.375 1.25 -0.21650635094611)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2.75 0.5 -0.43301270189222,2.75597796157458 0.457022294808167 -0.455760312055479,2.76522760095779 0.414850974295674 -0.478918395607368,2.77770451483146 0.37368848382115 -0.502375781105475,2.79334880719819 0.333732425808347 -0.526019860294666,2.81208537691484 0.295174611146642 -0.549737128688177,2.83382427821898 0.258200138394064 -0.573413730451116,2.85846115251769 0.222986505203176 -0.596936004970611,2.88587772936573 0.189702756235451 -0.620191032488805,2.91594239422828 0.15850867165462 -0.643067176179329,2.94851082030252 0.129554000094665 -0.66545461806501,2.98342666136509 0.102977739784634 -0.687245886204105,3.02052230231928 0.078907471281269 -0.708336370614292,3.05961966383903 0.057458745012683 -0.728624825457685,3.10053105724686 0.038734526573237 -0.748013855076124,3.14306008552201 0.022824702432479 -0.766410381543502,3.18700258611332 0.009805648431019 -0.783726091490617,3.23214761103096 -0.00026013686519 -0.79987786005755,3.27827843951188 -0.007324332192091 -0.814788149938374,3.32517361839783 -0.011353025555964 -0.828385383602543,3.37260802523135 -0.012326877029509 -0.84060428690611,3.42035394896652 -0.010241211594056 -0.851386202443257,3.46818218310621 -0.0051060415822 -0.860679371133851,3.51586312601842 0.003053981386852 -0.868439180695292,3.56316788314946 0.014199684748517 -0.874628379805819,3.60986936584274 0.028277563042523 -0.879217256931186,3.65574338148826 0.045220034768334 -0.882183782956251,3.70056970976957 0.064945766813234 -0.883513716936746,3.74413315984144 0.087360064895672 -0.883200674463593,3.78622460336345 0.112355328149502 -0.881246158311542,3.82664197843012 0.139811565666877 -0.87765955122504,3.86519125957839 0.169596972520106 -0.872458070875934,3.90168738921576 0.201568562497258 -0.865666687209244,3.93595516599773 0.235572854514029 -0.857318002573801,3.96783008588991 0.271446609406727 -0.847452095213172,3.99715913187718 0.30901761356937 -0.836116326868217,4.02380150852887 0.348105505672996 -0.823365115414876,4.04762931789371 0.3885226424985 -0.809259673628662,4.06852817347981 0.430074999726512 -0.793867715329923,4.08639774937224 0.472563103360048 -0.777263130320539,4.10115226185219 0.515782987308575 -0.759525629672545,4.1127208812057 0.559527172536592 -0.740740363071464,4.12104807174486 0.603585663076254 -0.720997510051345,4.12609385840951 0.647746954122649 -0.700391847083764,4.1278340186693 0.691799047372318 -0.679022292599034,4.12626019880507 0.735530468730831 -0.656991432123733,4.12137995401128 0.778731283503886 -0.634405025814187,4.11321671212692 0.821194104198432 -0.61137150074998,4.10180966116915 0.862715086095846 -0.588001430424789,4.08721356120934 0.903094905817856 -0.564407003933257,4.06949848149492 0.942139718187561 -0.540701487402092,4.04874946407869 0.979662086792094 -0.516998680250836,4.02506611557057 1.01548188377972 -0.493412368892526,3.99856212897149 1.04942715457186 -0.470055780496802,3.96936473788492 1.08133494333891 -0.447041039437686,3.93761410572614 1.11105207527721 -0.424478629035376,3.9034626528613 1.13843589193168 -0.402476861176005,3.86707432490649 1.16335493603432 -0.381141356355458,3.8286238056993 1.1856895825708 -0.360574536643342,3.78829567872099 1.20533261304587 -0.340875134001137,3.74628354099508 1.22218973019068 -0.322137716314888,3.70278907371594 1.23618001064108 -0.304452233417734,3.65802107406893 1.247236293414 -0.287903585281599,3.61219445288998 1.25530550231681 -0.27257121445099,3.56552920297626 1.26034890074202 -0.258528724675415,3.51824934300062 1.26234227762414 -0.245843527571206,3.47058184209972 1.26127606366599 -0.234576519008966,3.42275553029828 1.2571553772766 -0.224781786780142,3.375 1.25 -0.21650635094611)",
    )

    # OGRFeature(entities):62
    #   EntityHandle (String) = 204
    #   POLYGON Z ((2.125 -0.25 -0.649519052838329,2.875 -0.75 -1.08253175473055,3.5 0.0 -0.866025403784439,2.75 0.5 -0.43301270189222,2.125 -0.25 -0.649519052838329))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((2.125 -0.25 -0.649519052838329,2.875 -0.75 -1.08253175473055,3.5 0.0 -0.866025403784439,2.75 0.5 -0.43301270189222,2.125 -0.25 -0.649519052838329))",
    )

    # OGRFeature(entities):63
    #   EntityHandle (String) = 205
    #   POLYGON Z ((4.875 0.25 -1.08253175473055,5.5 1.0 -0.866025403784439,4.75 1.5 -0.43301270189222,4.125 0.75 -0.649519052838329,4.875 0.25 -1.08253175473055))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((4.875 0.25 -1.08253175473055,5.5 1.0 -0.866025403784439,4.75 1.5 -0.43301270189222,4.125 0.75 -0.649519052838329,4.875 0.25 -1.08253175473055))",
    )

    # OGRFeature(entities):64
    #   EntityHandle (String) = 207
    #   POLYGON Z ((11 2 -1.73205080756888,11.625 2.75 -1.51554445662277,12.375 2.25 -1.94855715851499,11.75 1.5 -2.1650635094611,11 2 -1.73205080756888))
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON Z ((11 2 -1.73205080756888,11.625 2.75 -1.51554445662277,12.375 2.25 -1.94855715851499,11.75 1.5 -2.1650635094611,11 2 -1.73205080756888))",
    )

    # OGRFeature(entities):65
    #   EntityHandle (String) = 208
    #   LINESTRING Z (2.75 0.5 -0.43301270189222,2.96153846153846 0.538461538461539 -0.466321371268544,3.17307692307692 0.576923076923077 -0.499630040644869,3.38461538461539 0.615384615384616 -0.532938710021194,3.59615384615385 0.653846153846154 -0.566247379397518,3.80769230769231 0.692307692307693 -0.599556048773843,4.01923076923077 0.730769230769231 -0.632864718150167,4.23076923076923 0.76923076923077 -0.666173387526492,4.44230769230769 0.807692307692308 -0.699482056902817,4.65384615384616 0.846153846153847 -0.732790726279141,4.86538461538462 0.884615384615385 -0.766099395655466,5.07692307692308 0.923076923076924 -0.79940806503179,5.28846153846154 0.961538461538462 -0.832716734408115,5.5 1.0 -0.866025403784439,5.71153846153846 1.03846153846154 -0.899334073160764,5.92307692307693 1.07692307692308 -0.932642742537089,6.13461538461539 1.11538461538462 -0.965951411913413,6.34615384615385 1.15384615384615 -0.999260081289738,6.55769230769231 1.19230769230769 -1.03256875066606,6.76923076923077 1.23076923076923 -1.06587742004239,6.98076923076923 1.26923076923077 -1.09918608941871,7.19230769230769 1.30769230769231 -1.13249475879504,7.40384615384616 1.34615384615385 -1.16580342817136,7.61538461538462 1.38461538461539 -1.19911209754769,7.82692307692308 1.42307692307692 -1.23242076692401,8.03846153846154 1.46153846153846 -1.26572943630033,8.25 1.5 -1.29903810567666,8.46153846153846 1.53846153846154 -1.33234677505298,8.67307692307693 1.57692307692308 -1.36565544442931,8.88461538461539 1.61538461538462 -1.39896411380563,9.09615384615385 1.65384615384615 -1.43227278318196,9.30769230769231 1.69230769230769 -1.46558145255828,9.51923076923077 1.73076923076923 -1.49889012193461,9.73076923076923 1.76923076923077 -1.53219879131093,9.94230769230769 1.80769230769231 -1.56550746068726,10.1538461538462 1.84615384615385 -1.59881613006358,10.3653846153846 1.88461538461539 -1.6321247994399,10.5769230769231 1.92307692307692 -1.66543346881623,10.7884615384615 1.96153846153846 -1.69874213819255,11.0 2.0 -1.73205080756888)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (2.75 0.5 -0.43301270189222,2.96153846153846 0.538461538461539 -0.466321371268544,3.17307692307692 0.576923076923077 -0.499630040644869,3.38461538461539 0.615384615384616 -0.532938710021194,3.59615384615385 0.653846153846154 -0.566247379397518,3.80769230769231 0.692307692307693 -0.599556048773843,4.01923076923077 0.730769230769231 -0.632864718150167,4.23076923076923 0.76923076923077 -0.666173387526492,4.44230769230769 0.807692307692308 -0.699482056902817,4.65384615384616 0.846153846153847 -0.732790726279141,4.86538461538462 0.884615384615385 -0.766099395655466,5.07692307692308 0.923076923076924 -0.79940806503179,5.28846153846154 0.961538461538462 -0.832716734408115,5.5 1.0 -0.866025403784439,5.71153846153846 1.03846153846154 -0.899334073160764,5.92307692307693 1.07692307692308 -0.932642742537089,6.13461538461539 1.11538461538462 -0.965951411913413,6.34615384615385 1.15384615384615 -0.999260081289738,6.55769230769231 1.19230769230769 -1.03256875066606,6.76923076923077 1.23076923076923 -1.06587742004239,6.98076923076923 1.26923076923077 -1.09918608941871,7.19230769230769 1.30769230769231 -1.13249475879504,7.40384615384616 1.34615384615385 -1.16580342817136,7.61538461538462 1.38461538461539 -1.19911209754769,7.82692307692308 1.42307692307692 -1.23242076692401,8.03846153846154 1.46153846153846 -1.26572943630033,8.25 1.5 -1.29903810567666,8.46153846153846 1.53846153846154 -1.33234677505298,8.67307692307693 1.57692307692308 -1.36565544442931,8.88461538461539 1.61538461538462 -1.39896411380563,9.09615384615385 1.65384615384615 -1.43227278318196,9.30769230769231 1.69230769230769 -1.46558145255828,9.51923076923077 1.73076923076923 -1.49889012193461,9.73076923076923 1.76923076923077 -1.53219879131093,9.94230769230769 1.80769230769231 -1.56550746068726,10.1538461538462 1.84615384615385 -1.59881613006358,10.3653846153846 1.88461538461539 -1.6321247994399,10.5769230769231 1.92307692307692 -1.66543346881623,10.7884615384615 1.96153846153846 -1.69874213819255,11.0 2.0 -1.73205080756888)",
    )

    # OGRFeature(entities):66
    #   EntityHandle (String) = 209
    #   LINESTRING Z (5.75 5.50000000000001 1.29903810567666,5.5082446180819 5.22760624757667 1.22405710092841,5.26788002157825 4.95546536522868 1.14885710427104,5.03029699590351 4.68383022303141 1.07321912379545,4.79688632647213 4.4129536910602 0.996924167592515,4.56903879869856 4.14308863939042 0.91975324375312,4.34814519799727 3.87448793809741 0.841487360368152,4.1355963097827 3.60740445725653 0.761907525528497,3.93278291946931 3.34209106694314 0.68079474732504,3.74109581247155 3.07880063723259 0.597930033848667,3.56192577420388 2.81778603820024 0.513094393190262,3.39666359008075 2.55930013992144 0.426068833440712,3.24670004551661 2.30359581247155 0.336634362690902,3.11342592592593 2.05092592592593 0.244571989031716,2.99823201672314 1.80154335035992 0.149662720554042,2.90250910332271 1.55570095584889 0.051687565348764,2.8276479711391 1.31365161246818 -0.049572468493233,2.77503940558674 1.07564819029316 -0.154336372881063,2.74607419208011 0.84194355939918 -0.26282313972384,2.74214311603365 0.612790589861594 -0.375251760930679,2.76457901346955 0.38843161550262 -0.491832103747815,2.8133818843878 0.168866636322256 -0.612564168175248,2.88721889276623 -0.046146681501713 -0.737238086966744,2.98469925319038 -0.256861208044643 -0.865634868213187,3.10443218024579 -0.463529813381887 -0.997535520005462,3.24502688851802 -0.666405367588803 -1.13272105043446,3.4050925925926 -0.865740740740744 -1.27097246759105,3.58323850705508 -1.06178880291307 -1.41207077956614,3.77807384649101 -1.25480242418113 -1.5557969944506,3.98820782548594 -1.44503447462028 -1.70193212033533,4.21224965862541 -1.63273782430588 -1.85025716531119,4.44880856049496 -1.81816534331328 -2.00055313746909,4.69649374568015 -2.00156990171784 -2.15260104489991,4.95391442876651 -2.18320436959491 -2.30618189569452,5.2196798243396 -2.36332161701985 -2.46107669794383,5.49239914698496 -2.54217451406801 -2.61706645973871,5.77068161128813 -2.72001593081475 -2.77393218917004,6.05313643183467 -2.89709873733543 -2.93145489432872,6.33837282321012 -3.0736758037054 -3.08941558330563,6.625 -3.25 -3.24759526419165)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "LINESTRING Z (5.75 5.50000000000001 1.29903810567666,5.5082446180819 5.22760624757667 1.22405710092841,5.26788002157825 4.95546536522868 1.14885710427104,5.03029699590351 4.68383022303141 1.07321912379545,4.79688632647213 4.4129536910602 0.996924167592515,4.56903879869856 4.14308863939042 0.91975324375312,4.34814519799727 3.87448793809741 0.841487360368152,4.1355963097827 3.60740445725653 0.761907525528497,3.93278291946931 3.34209106694314 0.68079474732504,3.74109581247155 3.07880063723259 0.597930033848667,3.56192577420388 2.81778603820024 0.513094393190262,3.39666359008075 2.55930013992144 0.426068833440712,3.24670004551661 2.30359581247155 0.336634362690902,3.11342592592593 2.05092592592593 0.244571989031716,2.99823201672314 1.80154335035992 0.149662720554042,2.90250910332271 1.55570095584889 0.051687565348764,2.8276479711391 1.31365161246818 -0.049572468493233,2.77503940558674 1.07564819029316 -0.154336372881063,2.74607419208011 0.84194355939918 -0.26282313972384,2.74214311603365 0.612790589861594 -0.375251760930679,2.76457901346955 0.38843161550262 -0.491832103747815,2.8133818843878 0.168866636322256 -0.612564168175248,2.88721889276623 -0.046146681501713 -0.737238086966744,2.98469925319038 -0.256861208044643 -0.865634868213187,3.10443218024579 -0.463529813381887 -0.997535520005462,3.24502688851802 -0.666405367588803 -1.13272105043446,3.4050925925926 -0.865740740740744 -1.27097246759105,3.58323850705508 -1.06178880291307 -1.41207077956614,3.77807384649101 -1.25480242418113 -1.5557969944506,3.98820782548594 -1.44503447462028 -1.70193212033533,4.21224965862541 -1.63273782430588 -1.85025716531119,4.44880856049496 -1.81816534331328 -2.00055313746909,4.69649374568015 -2.00156990171784 -2.15260104489991,4.95391442876651 -2.18320436959491 -2.30618189569452,5.2196798243396 -2.36332161701985 -2.46107669794383,5.49239914698496 -2.54217451406801 -2.61706645973871,5.77068161128813 -2.72001593081475 -2.77393218917004,6.05313643183467 -2.89709873733543 -2.93145489432872,6.33837282321012 -3.0736758037054 -3.08941558330563,6.625 -3.25 -3.24759526419165)",
    )

    # OGRFeature(entities):67
    #   EntityHandle (String) = 20A
    #   POINT Z (9.625 1.75 -1.51554445662277)
    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, "POINT Z (9.625 1.75 -1.51554445662277)")


###############################################################################
# Test 3D entities (polyface mesh, cylinder, 3D solid)


def test_ogr_dxf_33():

    with gdal.config_option("DXF_3D_EXTENSIBLE_MODE", "TRUE"):
        ds = ogr.Open("data/dxf/3d.dxf")

    layer = ds.GetLayer(0)

    # Polyface mesh (POLYLINE)
    feat = layer.GetNextFeature()
    assert feat.Layer == "0"

    geom = feat.GetGeometryRef()
    assert geom.GetGeometryType() == ogr.wkbPolyhedralSurfaceZ, (
        "did not get expected geometry type; got %s instead of wkbPolyhedralSurface"
        % geom.GetGeometryType()
    )

    wkt_string = geom.ExportToIsoWkt()
    wkt_string_expected = "POLYHEDRALSURFACE Z (((0 0 0,1 0 0,1 1 0,0 1 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 0 0,1 1 0,1 1 1,1 0 1,1 0 0)),((1 1 0,1 1 1,0 1 1,0 1 0,1 1 0)),((0 0 0,0 1 0,0 1 1,0 0 1,0 0 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))"
    assert wkt_string == wkt_string_expected, "wrong geometry for polyface mesh"

    faces = geom.GetGeometryCount()
    assert faces == 6, "did not get expected number of faces, got %d instead of %d" % (
        faces,
        6,
    )

    # Cylinder (CIRCLE with thickness)
    feat = layer.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYHEDRALSURFACE Z (((2.8 -0.0 1e-31,2.79902562010393 -0.0 -0.0279025894976501,2.79610722749663 -0.0 -0.0556692403840262,2.79125904029352 -0.0 -0.0831646763271037,2.78450467837533 -0.0 -0.1102549423268,2.77587704831436 -0.0 -0.136808057330267,2.76541818305704 -0.0 -0.16269465723032,2.75317903714357 -0.0 -0.187788625114356,2.73921923846257 -0.0 -0.211967705693282,2.72360679774998 0.0 -0.235114100916989,2.70641777724759 0.0 -0.257115043874616,2.68773592013546 0.0 -0.277863348183599,2.66765224254354 0.0 -0.297257930190958,2.64626459013026 0.0 -0.315204301442689,2.6236771613883 0.0 -0.331615029022017,2.6 0.0 -0.346410161513775,2.57534845871563 0.0 -0.359517618519667,2.54984263736636 0.0 -0.370873541826715,2.52360679774998 0.0 -0.380422606518061,2.49676875823987 0.0 -0.388118290510399,2.46945927106677 0.0 -0.393923101204883,2.44181138530706 0.0 -0.397808758147309,2.413959798681 0.0 -0.399756330807638,2.386040201319 0.0 -0.399756330807638,2.35818861469294 0.0 -0.397808758147309,2.33054072893323 0.0 -0.393923101204883,2.30323124176013 0.0 -0.388118290510399,2.27639320225002 0.0 -0.380422606518061,2.25015736263363 0.0 -0.370873541826715,2.22465154128437 0.0 -0.359517618519667,2.2 0.0 -0.346410161513776,2.1763228386117 0.0 -0.331615029022017,2.15373540986974 0.0 -0.315204301442689,2.13234775745646 0.0 -0.297257930190958,2.11226407986454 0.0 -0.277863348183599,2.09358222275241 0.0 -0.257115043874616,2.07639320225002 0.0 -0.235114100916989,2.06078076153743 0.0 -0.211967705693282,2.04682096285643 0.0 -0.187788625114356,2.03458181694296 0.0 -0.16269465723032,2.02412295168564 0.0 -0.136808057330268,2.01549532162467 0.0 -0.1102549423268,2.00874095970648 0.0 -0.0831646763271037,2.00389277250337 0.0 -0.0556692403840263,2.00097437989607 0.0 -0.0279025894976502,"
        + "2.0 0.0 -4.8985871965894e-17,2.00097437989607 0.0 0.0279025894976499,2.00389277250337 0.0 0.0556692403840262,2.00874095970648 0.0 0.0831646763271036,2.01549532162467 0.0 0.1102549423268,2.02412295168564 0.0 0.136808057330267,2.03458181694296 0.0 0.16269465723032,2.04682096285643 0.0 0.187788625114356,2.06078076153743 0.0 0.211967705693282,2.07639320225002 0.0 0.235114100916989,2.09358222275241 0.0 0.257115043874616,2.11226407986454 0.0 0.277863348183599,2.13234775745646 0.0 0.297257930190958,2.15373540986974 0.0 0.315204301442689,2.1763228386117 -0.0 0.331615029022017,2.2 -0.0 0.346410161513775,2.22465154128437 -0.0 0.359517618519667,2.25015736263363 -0.0 0.370873541826715,2.27639320225002 -0.0 0.380422606518061,2.30323124176013 -0.0 0.388118290510399,2.33054072893323 -0.0 0.393923101204883,2.35818861469294 -0.0 0.397808758147309,2.386040201319 -0.0 0.399756330807638,2.413959798681 -0.0 0.399756330807638,2.44181138530706 -0.0 0.397808758147309,2.46945927106677 -0.0 0.393923101204883,2.49676875823987 -0.0 0.388118290510399,2.52360679774998 -0.0 0.380422606518061,2.54984263736636 -0.0 0.370873541826715,2.57534845871563 -0.0 0.359517618519667,2.6 -0.0 0.346410161513775,2.6236771613883 -0.0 0.331615029022017,2.64626459013026 -0.0 0.315204301442689,2.66765224254354 -0.0 0.297257930190958,2.68773592013546 -0.0 0.277863348183599,2.70641777724759 -0.0 0.257115043874616,2.72360679774998 -0.0 0.235114100916989,2.73921923846257 -0.0 0.211967705693282,2.75317903714357 -0.0 0.187788625114356,2.76541818305704 -0.0 0.16269465723032,2.77587704831436 -0.0 0.136808057330267,2.78450467837533 -0.0 0.1102549423268,2.79125904029352 -0.0 0.0831646763271039,2.79610722749663 -0.0 0.0556692403840264,2.79902562010393 -0.0 0.0279025894976499,2.8 -0.0 1e-31)),"
        + "((2.8 1.8 3.6e-16,2.79902562010393 1.8 -0.0279025894976498,2.79610722749663 1.8 -0.0556692403840258,2.79125904029352 1.8 -0.0831646763271034,2.78450467837533 1.8 -0.110254942326799,2.77587704831436 1.8 -0.136808057330267,2.76541818305704 1.8 -0.16269465723032,2.75317903714357 1.8 -0.187788625114356,2.73921923846257 1.8 -0.211967705693282,2.72360679774998 1.8 -0.235114100916989,2.70641777724759 1.8 -0.257115043874615,2.68773592013546 1.8 -0.277863348183599,2.66765224254354 1.8 -0.297257930190957,2.64626459013026 1.8 -0.315204301442689,2.6236771613883 1.8 -0.331615029022016,2.6 1.8 -0.346410161513775,2.57534845871563 1.8 -0.359517618519667,2.54984263736636 1.8 -0.370873541826715,2.52360679774998 1.8 -0.380422606518061,2.49676875823987 1.8 -0.388118290510398,2.46945927106677 1.8 -0.393923101204883,2.44181138530706 1.8 -0.397808758147309,2.413959798681 1.8 -0.399756330807638,2.386040201319 1.8 -0.399756330807638,2.35818861469294 1.8 -0.397808758147309,2.33054072893323 1.8 -0.393923101204883,2.30323124176013 1.8 -0.388118290510398,2.27639320225002 1.8 -0.380422606518061,2.25015736263363 1.8 -0.370873541826715,2.22465154128437 1.8 -0.359517618519666,2.2 1.8 -0.346410161513775,2.1763228386117 1.8 -0.331615029022016,2.15373540986974 1.8 -0.315204301442689,2.13234775745646 1.8 -0.297257930190957,2.11226407986454 1.8 -0.277863348183599,2.09358222275241 1.8 -0.257115043874615,2.07639320225002 1.8 -0.235114100916989,2.06078076153743 1.8 -0.211967705693282,2.04682096285643 1.8 -0.187788625114356,2.03458181694296 1.8 -0.16269465723032,2.02412295168564 1.8 -0.136808057330267,2.01549532162467 1.8 -0.1102549423268,2.00874095970648 1.8 -0.0831646763271034,2.00389277250337 1.8 -0.0556692403840259,2.00097437989607 1.8 -0.0279025894976499,2.0 1.8 3.11014128034106e-16,"
        + "2.00097437989607 1.8 0.0279025894976503,2.00389277250337 1.8 0.0556692403840266,2.00874095970648 1.8 0.083164676327104,2.01549532162467 1.8 0.1102549423268,2.02412295168564 1.8 0.136808057330268,2.03458181694296 1.8 0.16269465723032,2.04682096285643 1.8 0.187788625114357,2.06078076153743 1.8 0.211967705693282,2.07639320225002 1.8 0.23511410091699,2.09358222275241 1.8 0.257115043874616,2.11226407986454 1.8 0.277863348183599,2.13234775745646 1.8 0.297257930190958,2.15373540986974 1.8 0.315204301442689,2.1763228386117 1.8 0.331615029022017,2.2 1.8 0.346410161513776,2.22465154128437 1.8 0.359517618519667,2.25015736263363 1.8 0.370873541826715,2.27639320225002 1.8 0.380422606518062,2.30323124176013 1.8 0.388118290510399,2.33054072893323 1.8 0.393923101204884,2.35818861469294 1.8 0.39780875814731,2.386040201319 1.8 0.399756330807639,2.413959798681 1.8 0.399756330807639,2.44181138530706 1.8 0.39780875814731,2.46945927106677 1.8 0.393923101204884,2.49676875823987 1.8 0.388118290510399,2.52360679774998 1.8 0.380422606518062,2.54984263736636 1.8 0.370873541826715,2.57534845871563 1.8 0.359517618519667,2.6 1.8 0.346410161513776,2.6236771613883 1.8 0.331615029022017,2.64626459013026 1.8 0.315204301442689,2.66765224254354 1.8 0.297257930190958,2.68773592013546 1.8 0.277863348183599,2.70641777724759 1.8 0.257115043874616,2.72360679774998 1.8 0.23511410091699,2.73921923846257 1.8 0.211967705693283,2.75317903714357 1.8 0.187788625114357,2.76541818305704 1.8 0.16269465723032,2.77587704831436 1.8 0.136808057330268,2.78450467837533 1.8 0.1102549423268,2.79125904029352 1.8 0.0831646763271043,2.79610722749663 1.8 0.0556692403840267,2.79902562010393 1.8 0.0279025894976503,2.8 1.8 3.6e-16)),"
        + "((2.0 0.0 -4.8985871965894e-17,2.00097437989607 0.0 -0.0279025894976502,2.00389277250337 0.0 -0.0556692403840263,2.00874095970648 0.0 -0.0831646763271037,2.01549532162467 0.0 -0.1102549423268,2.02412295168564 0.0 -0.136808057330268,2.03458181694296 0.0 -0.16269465723032,2.04682096285643 0.0 -0.187788625114356,2.06078076153743 0.0 -0.211967705693282,2.07639320225002 0.0 -0.235114100916989,2.09358222275241 0.0 -0.257115043874616,2.11226407986454 0.0 -0.277863348183599,2.13234775745646 0.0 -0.297257930190958,2.15373540986974 0.0 -0.315204301442689,2.1763228386117 0.0 -0.331615029022017,2.2 0.0 -0.346410161513776,2.22465154128437 0.0 -0.359517618519667,2.25015736263363 0.0 -0.370873541826715,2.27639320225002 0.0 -0.380422606518061,2.30323124176013 0.0 -0.388118290510399,2.33054072893323 0.0 -0.393923101204883,2.35818861469294 0.0 -0.397808758147309,2.386040201319 0.0 -0.399756330807638,2.413959798681 0.0 -0.399756330807638,2.44181138530706 0.0 -0.397808758147309,2.46945927106677 0.0 -0.393923101204883,2.49676875823987 0.0 -0.388118290510399,2.52360679774998 0.0 -0.380422606518061,2.54984263736636 0.0 -0.370873541826715,2.57534845871563 0.0 -0.359517618519667,2.6 0.0 -0.346410161513775,2.6236771613883 0.0 -0.331615029022017,2.64626459013026 0.0 -0.315204301442689,2.66765224254354 0.0 -0.297257930190958,2.68773592013546 0.0 -0.277863348183599,2.70641777724759 0.0 -0.257115043874616,2.72360679774998 0.0 -0.235114100916989,2.73921923846257 -0.0 -0.211967705693282,2.75317903714357 -0.0 -0.187788625114356,2.76541818305704 -0.0 -0.16269465723032,2.77587704831436 -0.0 -0.136808057330267,2.78450467837533 -0.0 -0.1102549423268,2.79125904029352 -0.0 -0.0831646763271037,2.79610722749663 -0.0 -0.0556692403840262,2.79902562010393 -0.0 -0.0279025894976501,"
        + "2.8 -0.0 1e-31,2.8 1.8 3.6e-16,2.79902562010393 1.8 -0.0279025894976498,2.79610722749663 1.8 -0.0556692403840258,2.79125904029352 1.8 -0.0831646763271034,2.78450467837533 1.8 -0.110254942326799,2.77587704831436 1.8 -0.136808057330267,2.76541818305704 1.8 -0.16269465723032,2.75317903714357 1.8 -0.187788625114356,2.73921923846257 1.8 -0.211967705693282,2.72360679774998 1.8 -0.235114100916989,2.70641777724759 1.8 -0.257115043874615,2.68773592013546 1.8 -0.277863348183599,2.66765224254354 1.8 -0.297257930190957,2.64626459013026 1.8 -0.315204301442689,2.6236771613883 1.8 -0.331615029022016,2.6 1.8 -0.346410161513775,2.57534845871563 1.8 -0.359517618519667,2.54984263736636 1.8 -0.370873541826715,2.52360679774998 1.8 -0.380422606518061,2.49676875823987 1.8 -0.388118290510398,2.46945927106677 1.8 -0.393923101204883,2.44181138530706 1.8 -0.397808758147309,2.413959798681 1.8 -0.399756330807638,2.386040201319 1.8 -0.399756330807638,2.35818861469294 1.8 -0.397808758147309,2.33054072893323 1.8 -0.393923101204883,2.30323124176013 1.8 -0.388118290510398,2.27639320225002 1.8 -0.380422606518061,2.25015736263363 1.8 -0.370873541826715,2.22465154128437 1.8 -0.359517618519666,2.2 1.8 -0.346410161513775,2.1763228386117 1.8 -0.331615029022016,2.15373540986974 1.8 -0.315204301442689,2.13234775745646 1.8 -0.297257930190957,2.11226407986454 1.8 -0.277863348183599,2.09358222275241 1.8 -0.257115043874615,2.07639320225002 1.8 -0.235114100916989,2.06078076153743 1.8 -0.211967705693282,2.04682096285643 1.8 -0.187788625114356,2.03458181694296 1.8 -0.16269465723032,2.02412295168564 1.8 -0.136808057330267,2.01549532162467 1.8 -0.1102549423268,2.00874095970648 1.8 -0.0831646763271034,2.00389277250337 1.8 -0.0556692403840259,2.00097437989607 1.8 -0.0279025894976499,2.0 1.8 3.11014128034106e-16,2.0 0.0 -4.8985871965894e-17)),"
        + "((2.8 -0.0 1e-31,2.79902562010393 -0.0 0.0279025894976499,2.79610722749663 -0.0 0.0556692403840264,2.79125904029352 -0.0 0.0831646763271039,2.78450467837533 -0.0 0.1102549423268,2.77587704831436 -0.0 0.136808057330267,2.76541818305704 -0.0 0.16269465723032,2.75317903714357 -0.0 0.187788625114356,2.73921923846257 -0.0 0.211967705693282,2.72360679774998 -0.0 0.235114100916989,2.70641777724759 -0.0 0.257115043874616,2.68773592013546 -0.0 0.277863348183599,2.66765224254354 -0.0 0.297257930190958,2.64626459013026 -0.0 0.315204301442689,2.6236771613883 -0.0 0.331615029022017,2.6 -0.0 0.346410161513775,2.57534845871563 -0.0 0.359517618519667,2.54984263736636 -0.0 0.370873541826715,2.52360679774998 -0.0 0.380422606518061,2.49676875823987 -0.0 0.388118290510399,2.46945927106677 -0.0 0.393923101204883,2.44181138530706 -0.0 0.397808758147309,2.413959798681 -0.0 0.399756330807638,2.386040201319 -0.0 0.399756330807638,2.35818861469294 -0.0 0.397808758147309,2.33054072893323 -0.0 0.393923101204883,2.30323124176013 -0.0 0.388118290510399,2.27639320225002 -0.0 0.380422606518061,2.25015736263363 -0.0 0.370873541826715,2.22465154128437 -0.0 0.359517618519667,2.2 -0.0 0.346410161513775,2.1763228386117 -0.0 0.331615029022017,2.15373540986974 0.0 0.315204301442689,2.13234775745646 0.0 0.297257930190958,2.11226407986454 0.0 0.277863348183599,2.09358222275241 0.0 0.257115043874616,2.07639320225002 0.0 0.235114100916989,2.06078076153743 0.0 0.211967705693282,2.04682096285643 0.0 0.187788625114356,2.03458181694296 0.0 0.16269465723032,2.02412295168564 0.0 0.136808057330267,2.01549532162467 0.0 0.1102549423268,2.00874095970648 0.0 0.0831646763271036,2.00389277250337 0.0 0.0556692403840262,2.00097437989607 0.0 0.0279025894976499,"
        + "2.0 0.0 -4.8985871965894e-17,2.0 1.8 3.11014128034106e-16,2.00097437989607 1.8 0.0279025894976503,2.00389277250337 1.8 0.0556692403840266,2.00874095970648 1.8 0.083164676327104,2.01549532162467 1.8 0.1102549423268,2.02412295168564 1.8 0.136808057330268,2.03458181694296 1.8 0.16269465723032,2.04682096285643 1.8 0.187788625114357,2.06078076153743 1.8 0.211967705693282,2.07639320225002 1.8 0.23511410091699,2.09358222275241 1.8 0.257115043874616,2.11226407986454 1.8 0.277863348183599,2.13234775745646 1.8 0.297257930190958,2.15373540986974 1.8 0.315204301442689,2.1763228386117 1.8 0.331615029022017,2.2 1.8 0.346410161513776,2.22465154128437 1.8 0.359517618519667,2.25015736263363 1.8 0.370873541826715,2.27639320225002 1.8 0.380422606518062,2.30323124176013 1.8 0.388118290510399,2.33054072893323 1.8 0.393923101204884,2.35818861469294 1.8 0.39780875814731,2.386040201319 1.8 0.399756330807639,2.413959798681 1.8 0.399756330807639,2.44181138530706 1.8 0.39780875814731,2.46945927106677 1.8 0.393923101204884,2.49676875823987 1.8 0.388118290510399,2.52360679774998 1.8 0.380422606518062,2.54984263736636 1.8 0.370873541826715,2.57534845871563 1.8 0.359517618519667,2.6 1.8 0.346410161513776,2.6236771613883 1.8 0.331615029022017,2.64626459013026 1.8 0.315204301442689,2.66765224254354 1.8 0.297257930190958,2.68773592013546 1.8 0.277863348183599,2.70641777724759 1.8 0.257115043874616,2.72360679774998 1.8 0.23511410091699,2.73921923846257 1.8 0.211967705693283,2.75317903714357 1.8 0.187788625114357,2.76541818305704 1.8 0.16269465723032,2.77587704831436 1.8 0.136808057330268,2.78450467837533 1.8 0.1102549423268,2.79125904029352 1.8 0.0831646763271043,2.79610722749663 1.8 0.0556692403840267,2.79902562010393 1.8 0.0279025894976503,2.8 1.8 3.6e-16,2.8 -0.0 1e-31)))",
    ), "wrong geometry for cylinder"

    # 3DSOLID, plain
    feat = layer.GetNextFeature()

    assert feat.GetGeometryRef() is None, "geometry on first 3DSOLID was not empty"

    assert (
        feat.GetFieldAsBinary("ASMData")
        == b"ACIS BinaryFile(U\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x0c\x00\x00\x00\x07\x10Autodesk AutoCAD\x07\x13ASM 221.0.0.1871 NT\x07\x18Sun Mar 04 15:10:20 2018\x06ffffff9@\x06\x8d\xed\xb5\xa0\xf7\xc6\xb0>\x06\xbb\xbd\xd7\xd9\xdf|\xdb=\r\tasmheader\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x07\x0c221.0.0.1871\x11\r\x04body\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\x02\x00\x00\x00\x0c\xff\xff\xff\xff\x0c\x03\x00\x00\x00\x11\r\x04lump\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\x04\x00\x00\x00\x0c\x01\x00\x00\x00\x11\r\ttransform\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x14\x00\x00\x00\x00\x00\x00\xf0?\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0?\x00\x00\x00\x00\x00\x00\x00\x00\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0?\x14\x00\x00\x00\x00\x00\x00\x10@\x00\x00\x00\x00\x00\x00\xf0?\x00\x00\x00\x00\x00\x00\x00\x00\x06\x00\x00\x00\x00\x00\x00\xf0?\x0b\x0b\x0b\x11\r\x05shell\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\x05\x00\x00\x00\x0c\xff\xff\xff\xff\x0c\x02\x00\x00\x00\x11\r\x04face\x0c\x06\x00\x00\x00\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\x04\x00\x00\x00\x0c\xff\xff\xff\xff\x0c\x07\x00\x00\x00\x0b\x0b\x11\x0e\tpersubent\x0e\x10acadSolidHistory\r\x06attrib\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\x05\x00\x00\x00\x04\x01\x00\x00\x00\x04\x02\x00\x00\x00\x04\x01\x00\x00\x00\x04\x00\x00\x00\x00\x11\x0e\x06sphere\r\x07surface\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x13\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x06\xcd;\x7ff\x9e\xa0\xe6?\x14\x00\x00\x00\x00\x00\x00\xf0?\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0?\x0b\x0b\x0b\x0b\x0b\x11\x0e\x03End\x0e\x02of\x0e\x03ASM\r\x04data"
    ), "wrong ASMData on first 3DSOLID"

    assert feat.GetField("ASMTransform") == [
        1,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
    ], "wrong ASMTransform on first 3DSOLID"

    assert (
        feat.GetStyleString() == "BRUSH(fc:#000000)"
    ), "wrong style string on first 3DSOLID"

    # 3DSOLID inside a block
    feat = layer.GetNextFeature()

    assert feat.GetGeometryRef() is None, "geometry on second 3DSOLID was not empty"

    assert (
        feat.GetFieldAsBinary("ASMData")
        == b"ACIS BinaryFile(U\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x0c\x00\x00\x00\x07\x10Autodesk AutoCAD\x07\x13ASM 221.0.0.1871 NT\x07\x18Sun Mar 04 15:10:20 2018\x06ffffff9@\x06\x8d\xed\xb5\xa0\xf7\xc6\xb0>\x06\xbb\xbd\xd7\xd9\xdf|\xdb=\r\tasmheader\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x07\x0c221.0.0.1871\x11\r\x04body\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\x02\x00\x00\x00\x0c\xff\xff\xff\xff\x0c\x03\x00\x00\x00\x11\r\x04lump\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\x04\x00\x00\x00\x0c\x01\x00\x00\x00\x11\r\ttransform\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x14\x00\x00\x00\x00\x00\x00\xf0?\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0?\x00\x00\x00\x00\x00\x00\x00\x00\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0?\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x06\x00\x00\x00\x00\x00\x00\xf0?\x0b\x0b\x0b\x11\r\x05shell\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\x05\x00\x00\x00\x0c\xff\xff\xff\xff\x0c\x02\x00\x00\x00\x11\r\x04face\x0c\x06\x00\x00\x00\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\x04\x00\x00\x00\x0c\xff\xff\xff\xff\x0c\x07\x00\x00\x00\x0b\x0b\x11\x0e\tpersubent\x0e\x10acadSolidHistory\r\x06attrib\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x0c\x05\x00\x00\x00\x04\x01\x00\x00\x00\x04\x04\x00\x00\x00\x04\x01\x00\x00\x00\x04\x00\x00\x00\x00\x11\x0e\x06sphere\r\x07surface\x0c\xff\xff\xff\xff\x04\xff\xff\xff\xff\x0c\xff\xff\xff\xff\x13\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x06\x00\x00\x00\x00\x00\x00\x00@\x14\x00\x00\x00\x00\x00\x00\xf0?\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x14\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0?\x0b\x0b\x0b\x0b\x0b\x11\x0e\x03End\x0e\x02of\x0e\x03ASM\r\x04data"
    ), "wrong ASMData on second 3DSOLID"

    assert feat.GetField("ASMTransform") == pytest.approx(
        [
            -0.1875,
            0.3247595264191645,
            0.0,
            0.08660254037844387,
            0.05,
            0.0,
            0.0,
            0.0,
            -1.0,
            5.75,
            1.125,
            0.0,
        ]
    ), "wrong ASMTransform on second 3DSOLID"

    assert (
        feat.GetStyleString() == "BRUSH(fc:#ff0000)"
    ), "wrong style string on second 3DSOLID"

    # 3DSOLID inside a block where the INSERT has rotation and OCS
    feat = layer.GetNextFeature()

    assert feat.GetField("ASMTransform") == pytest.approx(
        [0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 100.0, 200.0, 300.0]
    ), "wrong ASMTransform on third 3DSOLID"


###############################################################################
# Writing Triangle geometry and checking if it is written properly


def test_ogr_dxf_34(tmp_path):
    ds = ogr.GetDriverByName("DXF").CreateDataSource(tmp_path / "triangle_test.dxf")
    lyr = ds.CreateLayer("entities")
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(
        ogr.CreateGeometryFromWkt("TRIANGLE ((0 0,0 1,1 0,0 0))")
    )

    lyr.CreateFeature(dst_feat)
    dst_feat = None

    lyr = None
    ds = None

    # Read back.
    ds = ogr.Open(tmp_path / "triangle_test.dxf")
    lyr = ds.GetLayer(0)

    # Check first feature
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    expected_wkt = "POLYGON ((0 0,0 1,1 0,0 0))"
    received_wkt = geom.ExportToWkt()

    assert expected_wkt == received_wkt, (
        "did not get expected geometry back: got %s" % received_wkt
    )
    ds = None


###############################################################################
# Test reading hatch with elliptical harts


def test_ogr_dxf_35():

    ds = ogr.Open("data/dxf/elliptical-arc-hatch-min.dxf")
    lyr = ds.GetLayer(0)

    expected_wkt = (
        "POLYGON Z ((10.0 5.0 0,10.0121275732481 0.823574944937595 0,"
        + "10.0484514617793 -3.3325901498166 0,"
        + "10.1087954573461 -7.44833360561541 0,"
        + "10.1928668294578 -11.5036898303666 0,"
        + "10.3002577454253 -15.478986172205 0,"
        + "10.4304472487686 -19.3549383521031 0,"
        + "10.5828037863926 -23.1127440124738 0,"
        + "10.7565882722693 -26.7341739279578 0,"
        + "10.950957672766 -30.2016604359299 0,"
        + "11.164969096226 -33.4983826577451 0,"
        + "11.3975843669637 -36.6083480973141 0,"
        + "11.6476750614854 -39.5164702211696 0,"
        + "11.9140279825044 -42.2086416436832 0,"
        + "12.1953510441969 -44.6718025624057 0,"
        + "12.4902795401481 -46.8940041115515 0,"
        + "12.797382763583 -48.8644663262969 0,"
        + "13.1151709477668 -50.5736304367052 0,"
        + "13.442102492907 -52.0132052375995 0,"
        + "13.7765914444993 -53.1762073094422 0,"
        + "14.1170151868394 -54.0569948951078 0,"
        + "14.4617223143788 -54.6512952682117 0,"
        + "14.8090406427424 -54.9562254602315 0,"
        + "15.1572853205429 -54.9703062458714 0,"
        + "15.5047670026452 -54.6934693188278 0,"
        + "15.8498000452284 -54.1270576231449 0,"
        + "16.1907106828936 -53.2738188385539 0,"
        + "16.5258451481492 -52.137892051398 0,"
        + "16.853577693885 -50.7247876758036 0,"
        + "17.1723184799194 -49.0413607224995 0,"
        + "17.4805212853603 -47.0957775449594 0,"
        + "17.7766910093686 -44.8974762241817 0,"
        + "18.0593909239355 -42.457120784282 0,"
        + "18.3272496434925 -39.7865494609998 0,"
        + "18.578967777543 -36.8987172740701 0,"
        + "18.8133242340436 -33.8076331820431 0,"
        + "19.0291821429573 -30.5282921244156 0,"
        + "19.2254943712436 -27.0766022807403 0,"
        + "19.4013086025311 -23.4693078995808 0,"
        + "19.5557719568327 -19.7239080716712 0,"
        + "19.6881351278911 -15.8585718413141 0,"
        + "19.7977560180852 -11.8920500678142 0,"
        + "19.8841028532649 -7.84358446451107 0,"
        + "19.9467567624029 -3.73281425666327 0,"
        + "19.9854138095503 0.420319089008591 0,"
        + "19.9998864682387 4.59566860096071 0,"
        + "19.9901045311767 8.77297953637629 0,"
        + "19.9561154508277 12.9319876375154 0,"
        + "19.8980841092162 17.0525174342237 0,"
        + "19.8162920180808 21.1145801157289 0,"
        + "19.7111359532519 25.0984704969476 0,"
        + "19.5831260298811 28.9848626089156 0,"
        + "19.4328832278572 32.7549034496293 0,"
        + "19.2611363794148 36.390304440511 0,"
        + "19.0687186335478 39.8734301448397 0,"
        + "18.856563414379 43.1873838177704 0,"
        + "18.6256998930927 46.3160893729396 0,"
        + "18.3772479953948 49.2443693680303 0,"
        + "18.1124129687203 51.9580186309899 0,"
        + "17.8324795355432 54.4438731697361 0,"
        + "17.5388056611497 56.6898740310677 0,"
        + "17.2328159661089 58.6851257990001 0,"
        + "16.9159948153966 60.4199494487478 0,"
        + "16.5898791176976 61.8859292999569 0,"
        + "16.2560508698165 63.075953841417 0,"
        + "15.9161294823645 63.9842502292107 0,"
        + "15.5717639239516 64.6064122909483 0,"
        + "15.2246247219914 64.9394219002396 0,"
        + "14.8763958589235 64.9816636177129 0,"
        + "14.5287666031655 64.7329325275591 0,"
        + "14.1834233144223 64.1944352315841 0,"
        + "13.8420412631059 63.3687839959484 0,"
        + "13.5062765035495 62.2599840789869 0,"
        + "13.1777578404393 60.8734143015838 0,"
        + "12.8580789274355 59.2158009543548 0,"
        + "12.5487905363114 57.2951851682141 0,"
        + "12.2513930341143 55.1208839066128 0,"
        + "11.9673291048407 52.7034447686764 0,"
        + "11.6979767509346 50.0545948224921 0,"
        + "11.4446426085565 47.1871837167582 0,"
        + "11.2085556090535 44.1151213467616 0,"
        + "10.9908610173775 40.8533103770683 0,"
        + "10.792614876371 37.4175739482608 0,"
        + "10.6147788838724 33.8245789184198 0,"
        + "10.4582157274918 30.0917550117071 0,"
        + "10.323684899688 26.2372102662631 0,"
        + "10.2118390134483 22.2796431915832 0,"
        + "10.1232206364439 18.2382520615003 0,"
        + "10.0582596590181 14.1326417827963 0,"
        + "10.0172712087756 9.98272879122447 0,"
        + "13.3027626235603 8.26630944469236 0,"
        + "10.0 5.0 0))"
        ""
    )

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, expected_wkt)

    expected_wkt = (
        "POLYGON Z ((10.0172712087756 9.98272879122439 0,"
        + "10.0582596590181 14.1326417827963 0,"
        + "10.1232206364439 18.2382520615002 0,"
        + "10.2118390134483 22.2796431915831 0,"
        + "10.323684899688 26.237210266263 0,"
        + "10.4582157274918 30.0917550117071 0,"
        + "10.6147788838723 33.8245789184198 0,"
        + "10.792614876371 37.4175739482608 0,"
        + "10.9908610173775 40.8533103770683 0,"
        + "11.2085556090535 44.1151213467616 0,"
        + "11.4446426085564 47.1871837167582 0,"
        + "11.6979767509346 50.0545948224921 0,"
        + "11.9673291048407 52.7034447686763 0,"
        + "12.2513930341143 55.1208839066127 0,"
        + "12.5487905363114 57.295185168214 0,"
        + "12.8580789274355 59.2158009543548 0,"
        + "13.1777578404393 60.8734143015838 0,"
        + "13.5062765035495 62.2599840789869 0,"
        + "13.8420412631059 63.3687839959484 0,"
        + "14.1834233144223 64.1944352315841 0,"
        + "14.5287666031655 64.7329325275591 0,"
        + "14.8763958589235 64.9816636177129 0,"
        + "15.2246247219914 64.9394219002396 0,"
        + "15.5717639239516 64.6064122909483 0,"
        + "15.9161294823645 63.9842502292107 0,"
        + "16.2560508698165 63.075953841417 0,"
        + "16.5898791176976 61.8859292999569 0,"
        + "16.9159948153966 60.4199494487478 0,"
        + "17.2328159661089 58.6851257990001 0,"
        + "17.5388056611497 56.6898740310677 0,"
        + "17.8324795355432 54.4438731697361 0,"
        + "18.1124129687203 51.95801863099 0,"
        + "18.3772479953948 49.2443693680303 0,"
        + "18.6256998930927 46.3160893729396 0,"
        + "18.856563414379 43.1873838177704 0,"
        + "19.0687186335478 39.8734301448397 0,"
        + "19.2611363794148 36.3903044405111 0,"
        + "19.4328832278572 32.7549034496293 0,"
        + "19.5831260298811 28.9848626089156 0,"
        + "19.7111359532519 25.0984704969477 0,"
        + "19.8162920180808 21.1145801157289 0,"
        + "19.8980841092162 17.0525174342238 0,"
        + "19.9561154508277 12.9319876375155 0,"
        + "19.9901045311767 8.77297953637629 0,"
        + "19.9998864682387 4.59566860096075 0,"
        + "19.9854138095503 0.420319089008538 0,"
        + "19.9467567624029 -3.73281425666325 0,"
        + "19.8841028532649 -7.84358446451108 0,"
        + "19.7977560180852 -11.8920500678142 0,"
        + "19.6881351278911 -15.8585718413141 0,"
        + "19.5557719568327 -19.7239080716712 0,"
        + "19.4013086025311 -23.4693078995808 0,"
        + "19.2254943712436 -27.0766022807403 0,"
        + "19.0291821429573 -30.5282921244156 0,"
        + "18.8133242340436 -33.8076331820431 0,"
        + "18.578967777543 -36.8987172740701 0,"
        + "18.3272496434925 -39.7865494609998 0,"
        + "18.0593909239355 -42.457120784282 0,"
        + "17.7766910093686 -44.8974762241817 0,"
        + "17.4805212853603 -47.0957775449594 0,"
        + "17.1723184799194 -49.0413607224995 0,"
        + "16.853577693885 -50.7247876758035 0,"
        + "16.5258451481492 -52.137892051398 0,"
        + "16.1907106828936 -53.2738188385539 0,"
        + "15.8498000452284 -54.1270576231449 0,"
        + "15.5047670026452 -54.6934693188278 0,"
        + "15.1572853205429 -54.9703062458714 0,"
        + "14.8090406427424 -54.9562254602315 0,"
        + "14.4617223143788 -54.6512952682117 0,"
        + "14.1170151868394 -54.0569948951078 0,"
        + "13.7765914444993 -53.1762073094422 0,"
        + "13.442102492907 -52.0132052375995 0,"
        + "13.1151709477668 -50.5736304367052 0,"
        + "12.797382763583 -48.8644663262969 0,"
        + "12.4902795401481 -46.8940041115515 0,"
        + "12.1953510441969 -44.6718025624057 0,"
        + "11.9140279825044 -42.2086416436832 0,"
        + "11.6476750614854 -39.5164702211696 0,"
        + "11.3975843669637 -36.6083480973141 0,"
        + "11.164969096226 -33.4983826577452 0,"
        + "10.950957672766 -30.2016604359299 0,"
        + "10.7565882722693 -26.7341739279578 0,"
        + "10.5828037863926 -23.1127440124739 0,"
        + "10.4304472487686 -19.3549383521031 0,"
        + "10.3002577454253 -15.4789861722049 0,"
        + "10.1928668294578 -11.5036898303666 0,"
        + "10.1087954573461 -7.44833360561536 0,"
        + "10.0484514617793 -3.33259014981659 0,"
        + "10.0121275732481 0.823574944937621 0,"
        + "10.0 5.0 0,"
        + "13.3027626235603 8.26630944469236 0,"
        + "10.0172712087756 9.98272879122439 0))"
    )

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(feat, expected_wkt)


###############################################################################
# Test reading files with only INSERT content (#7006)


def test_ogr_dxf_36():

    with gdal.config_option("DXF_MERGE_BLOCK_GEOMETRIES", "FALSE"):
        ds = ogr.Open("data/dxf/insert_only.dxf")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 5


###############################################################################
# Create a blocks layer only


def test_ogr_dxf_37(tmp_vsimem):

    ds = ogr.GetDriverByName("DXF").CreateDataSource(tmp_vsimem / "ogr_dxf_37.dxf")

    lyr = ds.CreateLayer("blocks")

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(dst_feat)
    dst_feat = None

    lyr = None
    ds = None

    # Read back.
    with gdal.config_option("DXF_INLINE_BLOCKS", "FALSE"):
        ds = ogr.Open(tmp_vsimem / "ogr_dxf_37.dxf")
    lyr = ds.GetLayerByName("blocks")

    # Check first feature
    feat = lyr.GetNextFeature()
    assert feat is not None
    ds = None


###############################################################################
# Test degenerated cases of SOLID (#7038)


def test_ogr_dxf_38():

    ds = ogr.Open("data/dxf/solid-less-than-4-vertices.dxf")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT (0 2)")
    assert f.GetStyleString() == "PEN(c:#000000)"

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "LINESTRING (0.5 2.0,1 2)")
    assert f.GetStyleString() == "PEN(c:#000000)"


###############################################################################
# Test correct reordering of vertices in SOLID (#7038, #7089)


def test_ogr_dxf_39():

    ds = ogr.Open("data/dxf/solid-vertex-ordering.dxf")
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POLYGON ((0 5,1.5 2.5,1.5 0.0,0.0 2.5,0 5))")

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "POLYGON Z ((-10 13 123,10 10 123,5 12 123,8 13 123,-10 13 123))"
    )


###############################################################################
# Test handing of OCS vs WCS for MTEXT (#7049)


def test_ogr_dxf_40():

    ds = ogr.Open("data/dxf/mtext-ocs-reduced.dxf")
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(5)
    ogrtest.check_feature_geometry(f, "POINT (320000.0 5815007.5 0)")


###############################################################################
# Test handing of OCS vs WCS for SOLID, HATCH and INSERT (#7077, #7098)


def test_ogr_dxf_41():

    ds = ogr.Open("data/dxf/ocs2wcs3.dxf")
    lyr = ds.GetLayer(0)

    # INSERT #1: OCS normal vector (0,0,-1)
    f = lyr.GetFeature(1)
    ogrtest.check_feature_geometry(f, "LINESTRING (45 20,25 20,25 40,45 40,45 20)")

    # INSERT #2: OCS normal vector (0,1/sqrt(2),-1/sqrt(2))
    f = lyr.GetFeature(3)
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING Z (10.0 18.0 -76.3675323681472,-10.0 18.0 -76.3675323681472,-10.0 32.142135623731 -62.2253967444162,10.0 32.142135623731 -62.2253967444162,10.0 18.0 -76.3675323681472)",
    )

    # INSERT #3: OCS normal vector (0.6,sqrt(8)/5,sqrt(8)/5) with
    # Y scale factor of 2 and rotation angle of 45 degrees
    f = lyr.GetFeature(5)
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING Z (49.7198871869889 -21.8420670839387 75.1721817670195,34.1976071850546 -17.0401066991021 86.8340855568821,41.9587471852111 -48.595846365317 110.157893136607,57.4810271871454 -53.3978067501536 98.4959893467447,49.7198871869889 -21.8420670839387 75.1721817670195)",
    )

    # HATCH
    f = lyr.GetFeature(7)
    expected_wkt = (
        "POLYGON Z ((-4.0 41.0121933088198 -132.936074863071,"
        + "-4.40490904691695 41.0186412752948 -132.929626896596,"
        + "-4.80797195119362 41.0379557758564 -132.910312396034,"
        + "-5.20735098749398 41.0700487479548 -132.878219423936,"
        + "-5.60122522671268 41.1147738668667 -132.833494305024,"
        + "-5.98779883832069 41.1719272128483 -132.776340959042,"
        + "-6.3653092782765 41.2412482008871 -132.707019971004,"
        + "-6.73203532517085 41.3224207688135 -132.625847403077,"
        + "-7.08630492796504 41.4150748183547 -132.533193353536,"
        + "-7.42650282954181 41.5187879025613 -132.429480269329,"
        + "-7.75107793130996 41.6330871519123 -132.315181019978,"
        + "-8.05855036528454 41.7574514303164 -132.190816741574,"
        + "-8.34751824139814 41.8913137111809 -132.05695446071,"
        + "-8.61666403927964 42.0340636627126 -131.914204509178,"
        + "-8.86476061535765 42.1850504306657 -131.763217741225,"
        + "-9.09067679789991 42.3435856058464 -131.604682566044,"
        + "-9.29338254447862 42.5089463628474 -131.439321809043,"
        + "-9.47195363834675 42.6803787556975 -131.267889416193,"
        + "-9.62557590231259 42.8571011554044 -131.091167016486,"
        + "-9.75354891089994 43.0383078137139 -130.909960358177,"
        + "-9.85528918386859 43.2231725368399 -130.725095635051,"
        + "-9.9303328465346 43.4108524524121 -130.537415719479,"
        + "-9.97833774476093 43.6004918524697 -130.347776319421,"
        + "-9.99908500497526 43.7912260949768 -130.157042076914,"
        + "-9.99248003210238 43.9821855460716 -129.966082625819,"
        + "-9.95855294086101 44.1724995450765 -129.775768626814,"
        + "-9.89745841845876 44.3613003741885 -129.586967797702,"
        + "-9.80947501931113 44.5477272147525 -129.400540957138,"
        + "1.0 44.5477272147525 -129.400540957138,"
        + "0.988343845952696 44.306453848479 -129.641814323412,"
        + "0.953429730181654 44.0663054100155 -129.881962761875,"
        + "0.895420438411614 43.828401582239 -130.119866589652,"
        + "0.814586436738996 43.5938515826157 -130.354416589275,"
        + "0.711304610594103 43.3637489915164 -130.584519180374,"
        + "0.586056507527265 43.1391666534406 -130.80910151845,"
        + "0.439426092011876 42.9211516749198 -131.027116496971,"
        + "0.272097022732443 42.7107205424238 -131.237547629467,"
        + "0.084849465052212 42.5088543830312 -131.43941378886,"
        + "-0.1214435464779 42.3164943899624 -131.631773781928,"
        + "-0.34582017860938 42.134537434302 -131.813730737589,"
        + "-0.587234283906729 41.9638318833721 -131.984436288519,"
        + "-0.844560278369735 41.8051736452522 -132.143094526639,"
        + "-1.11659838942566 41.6593024578878 -132.288965714003,"
        + "-1.40208024982283 41.5268984400915 -132.421369731799,"
        + "-1.69967481134424 41.4085789205144 -132.539689251376,"
        + "-2.00799455076879 41.3048955593753 -132.643372612515,"
        + "-2.32560193914507 41.216331776366 -132.731936395525,"
        + "-2.65101614421488 41.1433004967256 -132.804967675165,"
        + "-2.98271993473683 41.0861422259924 -132.862125945898,"
        + "-3.31916675451876 41.04512346241 -132.903144709481,"
        + "-3.65878793317664 41.0204354543892 -132.927832717502,"
        + "-4.0 41.0121933088198 -132.936074863071))"
    )
    ogrtest.check_feature_geometry(f, expected_wkt)

    # SOLID
    f = lyr.GetFeature(9)
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((-10.0 13.0 124,8.0 13.0 124,5.0 12.0 123,10.0 10.0 121,-10.0 13.0 124))",
    )


###############################################################################
# Test insertion of blocks within blocks (#7106)


def test_ogr_dxf_42():

    # Inlining, merging
    ds = ogr.Open("data/dxf/block-insert-order.dxf")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2, (
        "Defaults: Expected 2 features, found %d" % lyr.GetFeatureCount()
    )

    # No inlining, merging
    with gdal.config_option("DXF_INLINE_BLOCKS", "FALSE"):
        ds = ogr.Open("data/dxf/block-insert-order.dxf")

    lyr = ds.GetLayerByName("entities")
    assert lyr.GetFeatureCount() == 2, (
        "No inlining: Expected 2 features on entities, found %d" % lyr.GetFeatureCount()
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "POINT Z (8.0 2.5 6)"
    )  # geometry for first insertion point

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "POINT Z (-1 -2 -3)"
    )  # geometry for second insertion point

    lyr = ds.GetLayerByName("blocks")
    lyr.GetFeatureCount() == 6, (
        "No inlining: Expected 6 feature on blocks, found %d" % lyr.GetFeatureCount()
    )

    f = lyr.GetFeature(3)
    ogrtest.check_feature_geometry(
        f, "POINT Z (5 5 0)"
    )  # geometry for second insertion of BLOCK4 on BLOCK3

    f = lyr.GetFeature(4)
    ogrtest.check_feature_geometry(
        f, "POINT Z (-5.48795472456028 1.69774937525433 4.12310562561766)xxxxxxxx"
    )  # Wrong geometry for third insertion of BLOCK4 on BLOCK3

    assert f.GetField("BlockName") == "BLOCK4", "Wrong BlockName"
    assert f.GetField("BlockScale") == [0.4, 1.0, 1.5], "Wrong BlockScale"
    assert f.GetField("BlockAngle") == 40, "Wrong BlockAngle"
    assert f.GetField("BlockOCSNormal") == [
        0.6,
        0.565685424949238,
        0.565685424949238,
    ], "Wrong BlockOCSNormal"
    assert f.GetField("BlockOCSCoords") == [5, 5, 0], "Wrong BlockOCSCoords"
    assert f.GetField("Block") == "BLOCK3", "Wrong Block"

    # Inlining, no merging
    with gdal.config_option("DXF_MERGE_BLOCK_GEOMETRIES", "FALSE"):
        ds = ogr.Open("data/dxf/block-insert-order.dxf")

    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 4, (
        "Merging: Expected 4 features, found %d" % lyr.GetFeatureCount()
    )


###############################################################################
# Ensure recursively-included blocks don't fail badly


def test_ogr_dxf_43():

    ds = ogr.Open("data/dxf/insert-recursive-pair.dxf")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1


###############################################################################
# General tests of LEADER and MULTILEADER entities (#7111)


def test_ogr_dxf_44():

    with gdaltest.config_option("DXF_MAX_BSPLINE_CONTROL_POINTS", "1"):
        ds = ogr.Open("data/dxf/leader-mleader.dxf")
        lyr = ds.GetLayer(0)
        with gdal.quiet_errors():
            lyr.GetFeatureCount()
        assert gdal.GetLastErrorMsg().find("DXF_MAX_BSPLINE_CONTROL_POINTS") >= 0

    ds = ogr.Open("data/dxf/leader-mleader.dxf")
    lyr = ds.GetLayer(0)

    # LEADER with default arrowhead, plus a couple of DIMSTYLE overrides
    # (6.0 arrowhead size and 1.5 scale factor)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING Z (21 40 0,10 40 0,19.3125 34.6875 0,10.3125 34.6875 0,-13.5990791268758 34.6875 0)",
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "POLYGON Z ((21.0 41.5 0,30 40 0,21.0 38.5 0,21.0 41.5 0))"
    )

    # Skip text
    f = lyr.GetNextFeature()

    # Basic LEADER with no dimension style or override information
    f = lyr.GetNextFeature()
    assert f.GetStyleString() == "PEN(c:#ff0000)"
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING Z (-20.9782552979609 38.1443878852919 30,-12.2152357926375 44.793971841437 30,-13.7256166009765 49.0748560186272 30,-13.9025293262723 49.0416613258524 30)",
    )

    f = lyr.GetNextFeature()
    assert f.GetStyleString() == "BRUSH(fc:#ff0000)"
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((-20.9601206293303 38.1204894796201 30,-21.121645731992 38.035579873508 30,-20.9963899665916 38.1682862909638 30,-20.9601206293303 38.1204894796201 30))",
    )

    # LEADER with a custom arrowhead that consists of a polygon and line
    f = lyr.GetNextFeature()
    assert f.GetStyleString() == "PEN(c:#00ff00)"
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING Z (26.8 77.6 0,10 65 0,25 55 0,25 50 0,40 65 0,48 65 0,169.282571623465 65.0 0)",
    )

    f = lyr.GetNextFeature()
    assert f.GetStyleString() == "BRUSH(fc:#00ff00)"
    ogrtest.check_feature_geometry(
        f, "POLYGON ((27.2 80.4,30.4 82.8,32.8 79.6,29.6 77.2,27.2 80.4))"
    )

    f = lyr.GetNextFeature()
    assert f.GetStyleString() == "PEN(c:#00ff00)"
    ogrtest.check_feature_geometry(f, "LINESTRING Z (28.4 78.8 0,26.8 77.6 0)")

    # Check that the very long text string in the MTEXT entity associated
    # to this LEADER is captured correctly
    f = lyr.GetNextFeature()
    assert len(f.GetField("Text")) == 319, "Wrong text length: got %d" % len(
        f.GetField("Text")
    )

    # MULTILEADER with custom arrowhead
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "MULTILINESTRING ((26.8 32.6,10 20,25 10,25 5,40 20),(40 20,48 20))"
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "POLYGON ((27.2 35.4,30.4 37.8,32.8 34.6,29.6 32.2,27.2 35.4))"
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "LINESTRING Z (28.4 33.8 0,26.8 32.6 0)")

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT (50.0 22.0327421555252)")

    assert (
        f.GetStyleString()
        == 'LABEL(f:"Arial",t:"Basic Multileader",p:7,s:4g,c:#000000)'
    ), "Wrong style string on MULTILEADER text"

    # There are three LEADERs, followed by two MULTILEADERs, without arrowheads.
    # In the first LEADER/MULTILEADER, the arrowhead is set to an empty block.
    # In the second LEADER/MULTILEADER, the arrowhead is too large to be displayed.
    # The third LEADER has the arrow turned off (this isn't possible for MULTILEADER).
    # We just check each of these to make sure there is no polygon (arrowhead) feature.
    for x in range(3):
        f = lyr.GetNextFeature()
        geom = f.GetGeometryRef()
        assert geom.GetGeometryType() == ogr.wkbLineString25D, (
            "Unexpected LEADER geometry, expected wkbLineString25D on iteration %d" % x
        )

    for x in range(2):
        f = lyr.GetNextFeature()
        geom = f.GetGeometryRef()
        assert geom.GetGeometryType() == ogr.wkbMultiLineString, (
            "Unexpected MULTILEADER geometry, expected wkbMultiLineString on iteration %d"
            % x
        )

        f = lyr.GetNextFeature()
        geom = f.GetGeometryRef()
        assert geom.GetGeometryType() == ogr.wkbPoint, (
            "Unexpected MULTILEADER geometry, expected wkbPoint on iteration %d" % x
        )

    # MULTILEADER with multiple leader lines and formatted text
    f = lyr.GetNextFeature()
    assert f.GetStyleString() == "PEN(c:#0000ff)"
    ogrtest.check_feature_geometry(
        f,
        "MULTILINESTRING ((7.6425115795681 -8.00285406769102,18.2 -20.0),(19.2913880067389 -13.9367332958948,18.2 -20.0),(18.2 -20.0,38 -20),(54.8204921137545 -22.5800753657327,60.2227692307692 -20.0),(60.2227692307692 -20.0,52.2227692307692 -20.0))",
    )

    f = lyr.GetNextFeature()
    assert f.GetStyleString() == "BRUSH(fc:#0000ff)"
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((7.1420359016196 -8.4432726642857 0,5 -5 0,8.1429872575166 -7.56243547109634 0,7.1420359016196 -8.4432726642857 0))",
    )

    f = lyr.GetNextFeature()
    assert f.GetStyleString() == "BRUSH(fc:#0000ff)"
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((18.6352657907565 -13.8186312970179 0,20 -10 0,19.9475102227214 -14.0548352947716 0,18.6352657907565 -13.8186312970179 0))",
    )

    f = lyr.GetNextFeature()
    assert (
        f.GetStyleString()
        == 'LABEL(f:"Calibri",it:1,t:"wwmhyhuasmjekhosovikpigsvtippomllixhzkrpithawzztyqybthjqiobkrpcxngjfkepricimyjplksbzteotqgnkbprugrextpsnhhiorevsxjxzomzcnyrtphzgeibfljbsaikosjfrhrrhidjswmxeqjrvbllbjggjblzydcqpbuzjhgcaoflgskaabkuikiwqcmytgbaxbiukpqvqjtfinygjcakzfdyyejvvpdbwsftxzimjettbzkjmdqfigwyoghbsolhvlfunknprjmmrfxntjwmvonkxvmgsntczwcmbujxkwzykpaexmquoatsvjkbchlzptgedbjnbutvimbufrbhxpwotemzvrxtidzdtbbqywmurmdzsgqqyiyahgmnacmhrlpekufpgfruh",p:7,s:4g,w:40,c:#0000ff)'
    )
    ogrtest.check_feature_geometry(f, "POINT (40.0 -17.9846153846154)")

    # Rotated MULTILEADER with scaled block content, block attributes, and
    # different leader color
    f = lyr.GetNextFeature()
    assert f.GetStyleString() == "PEN(c:#ff00ff)"
    ogrtest.check_feature_geometry(
        f,
        "MULTILINESTRING ((-41.8919467995818 -22.8930851139176,-36.1215379759023 -17.6108145786645),(-36.1215379759023 -17.6108145786645,-44.0 -19.0))",
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((-40.7553616986189 -14.3661762772835,-44.6945927106677 -15.0607689879512,-44 -19,-40.0607689879512 -18.3054072893323,-40.7553616986189 -14.3661762772835),(-41.9142984770378 -17.0075519687798,-41.126452274628 -16.8686334266463,-40.9875337324945 -17.6564796290561,-41.7753799349043 -17.7953981711896,-41.9142984770378 -17.0075519687798),(-42.0532170191713 -16.2197057663701,-42.1921355613049 -15.4318595639603,-41.4042893588951 -15.2929410218268,-41.2653708167616 -16.0807872242365,-42.0532170191713 -16.2197057663701),(-42.7021446794476 -17.1464705109134,-42.563226137314 -17.9343167133231,-43.3510723397238 -18.0732352554567,-43.4899908818573 -17.2853890530469,-42.7021446794476 -17.1464705109134),(-42.8410632215811 -16.3586243085036,-43.6289094239909 -16.4975428506372,-43.7678279661244 -15.7096966482274,-42.9799817637146 -15.5707781060938,-42.8410632215811 -16.3586243085036))",
    )

    test_text = "Apples\u00b1"

    f = lyr.GetNextFeature()
    assert (
        f.GetStyleString()
        == 'LABEL(f:"Arial",t:"' + test_text + '",p:2,s:1g,c:#ff0000,a:10)'
    )
    assert f.GetField("Text") == test_text
    ogrtest.check_feature_geometry(f, "POINT Z (-42.7597068401767 -14.5165110820149 0)")

    # MULTILEADER with no dogleg
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTILINESTRING ((-2.39659963256204 -14.5201521575302,-3.98423252456234 -23.1105237601191),(-26.0282877045921 -20.4748699216691,-3.98423252456233 -23.1105237601191))",
    )

    for x in range(4):
        f = lyr.GetNextFeature()

    # MULTILEADER with no leader lines (block content only)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "MULTILINESTRING EMPTY")

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((-4.98423252456234 -22.1105237601191,-6.98423252456234 -22.1105237601191,-6.98423252456234 -24.1105237601191,-4.98423252456234 -24.1105237601191,-4.98423252456234 -22.1105237601191),(-5.78423252456234 -23.3105237601191,-5.38423252456234 -23.3105237601191,-5.38423252456234 -23.7105237601191,-5.78423252456234 -23.7105237601191,-5.78423252456234 -23.3105237601191),(-5.78423252456234 -22.9105237601191,-5.78423252456234 -22.5105237601191,-5.38423252456234 -22.5105237601191,-5.38423252456234 -22.9105237601191,-5.78423252456234 -22.9105237601191),(-6.18423252456234 -23.3105237601191,-6.18423252456234 -23.7105237601191,-6.58423252456234 -23.7105237601191,-6.58423252456234 -23.3105237601191,-6.18423252456234 -23.3105237601191),(-6.18423252456234 -22.9105237601191,-6.58423252456234 -22.9105237601191,-6.58423252456234 -22.5105237601191,-6.18423252456234 -22.5105237601191,-6.18423252456234 -22.9105237601191))",
    )

    f = lyr.GetNextFeature()

    # LEADER with spline path
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING Z (75 -5 0,75.3293039686015 -5.27450166567948 0,75.686184437139 -5.54808513378289 0,76.0669570707518 -5.8208730793178 0,76.4679375345795 -6.09298817729179 0,76.8854414937615 -6.36455310271241 0,77.3157846134373 -6.63569053058724 0,77.7552825587464 -6.90652313592384 0,78.2002509948283 -7.17717359372979 0,78.6470055868223 -7.44776457901266 0,79.091861999868 -7.71841876678001 0,79.5311358991048 -7.98925883203941 0,79.9611429496723 -8.26040744979843 0,80.3781988167098 -8.53198729506465 0,80.7786191653568 -8.80412104284562 0,81.1587196607529 -9.07693136814892 0,81.5148159680374 -9.35054094598211 0,81.8432237523498 -9.62507245135277 0,82.1402586788297 -9.90064855926846 0,82.4022364126165 -10.1773919447368 0,82.6254726188496 -10.4554252827652 0,82.8062829626685 -10.7348712483614 0,82.9409831092127 -11.0158525165329 0,83.0258887236216 -11.2984917622873 0,83.0573154710347 -11.5829116606322 0,83.0315790165916 -11.869234886575 0,82.9452821800198 -12.1575745539156 0,82.8004070385963 -12.447864666659 0,82.603711185096 -12.7398802214393 0,82.3621180817583 -13.033390692038 0,82.0825511908225 -13.3281655522369 0,81.7719339745283 -13.6239742758175 0,81.4371898951149 -13.9205863365615 0,81.0852424148219 -14.2177712082505 0,80.7230149958886 -14.515298364666 0,80.3574311005547 -14.8129372795898 0,79.9954141910594 -15.1104574268035 0,79.6438877296422 -15.4076282800887 0,79.3097751785426 -15.704219313227 0,79 -16 0)",
    )

    # MULTILEADER with spline path including an arrowhead on one leader line,
    # and text on an angle
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTILINESTRING ((97.9154085227223 -24.4884177083425,98.2307499443399 -23.8667044316857,98.5274844683239 -23.1977407715784,98.8076056908493 -22.4865892691047,99.0731072080911 -21.7383124653484,99.3259826162243 -20.9579729013935,99.568225511424 -20.1506331183241,99.8018294898652 -19.3213556572241,100.028788147723 -18.4752030591775,100.251095081172 -17.6172378652682,100.470743886388 -16.7525226165803,100.689728159546 -15.8861198541978,100.91004149682 -15.0230921192046,101.133677494386 -14.1685019526847,101.362629748419 -13.327411895722,101.598891855094 -12.5048844894007,101.844457410585 -11.7059822748045,102.101320011068 -10.9357677930177,102.371473252719 -10.199303585124,102.656910731711 -9.50165219220749,102.95962604422 -8.84787615535218,103.281612786421 -8.24303801564202,103.624864554489 -7.69220031416101,103.991374944599 -7.20042559199311,104.383137552927 -6.77277639022231,104.802145975646 -6.41431524993259,105.250393808933 -6.13010471220794,105.729874648962 -5.92520731813233,106.242582091908 -5.80468560878975,106.790509733946 -5.77360212526418,107.375651171252 -5.8370194086396,108.0 -6.0),(99.0 -4.0,99.2390786191346 -4.00918383080352,99.4787687119818 -4.01534615590692,99.7189331856537 -4.01916439926796,99.9594349472622 -4.02131598484443,100.200136903919 -4.02247833659411,100.440901962737 -4.02332887847475,100.681593030828 -4.02454503444416,100.922073015303 -4.02680422846008,101.162204823276 -4.03078388448032,101.401851361856 -4.03716142646263,101.640875538158 -4.0466142783648,101.879140259293 -4.0598198641446,102.116508432372 -4.07745560775981,102.352842964508 -4.1001989331682,102.588006762813 -4.12872726432755,102.821862734399 -4.16371802519564,103.054283542724 -4.20580126092676,103.285277318696 -4.25494915918985,103.514951346557 -4.31065239888725,103.743415181632 -4.37239063008777,103.970778379245 -4.43964350286021,104.19715049472 -4.51189066727337,104.422641083382 -4.58861177339606,104.647359700555 -4.66928647129708,104.871415901564 -4.75339441104525,105.094919241732 -4.84041524270936,105.317979276384 -4.92982861635822,105.540705560844 -5.02111418206063,105.763207650437 -5.11375158988541,105.985595100486 -5.20722048990135,106.207977466317 -5.30100053217725,106.430464303253 -5.39457136678194,106.653165166619 -5.4874126437842,106.876189611739 -5.57900401325284,107.099647193937 -5.66882512525668,107.323647468538 -5.7563556298645,107.548299990866 -5.84107517714513,107.773714316245 -5.92246341716736,108.0 -6.0))",
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((98.5006722379985 -24.8076524621295 0,96 -28 0,97.330144807446 -24.1691829545554 0,98.5006722379985 -24.8076524621295 0))",
    )

    f = lyr.GetNextFeature()
    assert f.GetStyleString() == 'LABEL(f:"Arial",t:"Splines",p:7,a:342,s:2g,c:#000000)'
    ogrtest.check_feature_geometry(f, "POINT (110.7043505591 -4.20673403616296)")

    # MULTILEADER with DIMBREAK
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTILINESTRING ((50.8917622404846 41.5635728657296,51.2877903403879 42.2579494192141),(51.9070696740577 43.3437639093041,54.3108962133801 47.5585173269448,55.9270734326513 48.2521008552884),(57.0757636753042 48.7450620367561,59.4256548786735 49.7535194092661),(60 50,60 50),(60 50,60 50),(60.625 50.0,61.875 50.0),(63.125 50.0,63.6 50.0))",
    )


###############################################################################
# Test linetype scaling (#7129) and parsing of complex linetypes (#7134)


def test_ogr_dxf_45():

    ds = ogr.Open("data/dxf/linetypes.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    assert feat.GetField("Linetype") == "DASHED2", "Got wrong linetype (1)"

    assert (
        feat.GetStyleString() == 'PEN(c:#000000,p:"12.5g 6.25g")'
    ), "Got wrong style string (1)"

    feat = lyr.GetNextFeature()
    assert feat.GetField("Linetype") == "DASHED2", "Got wrong linetype (2)"

    assert (
        feat.GetStyleString() == 'PEN(c:#000000,p:"0.625g 0.3125g")'
    ), "Got wrong style string (2)"

    feat = lyr.GetNextFeature()
    assert feat.GetField("Linetype") == "DASHED2_FLIPPED", "Got wrong linetype (3)"

    assert (
        feat.GetStyleString() == 'PEN(c:#000000,p:"0.625g 0.3125g")'
    ), "Got wrong style string (3)"

    feat = lyr.GetNextFeature()
    assert feat.GetField("Linetype") == "Drain_Pipe_Inv_100", "Got wrong linetype (4)"

    assert (
        feat.GetStyleString() == 'PEN(c:#000000,p:"35g 22.5g")'
    ), "Got wrong style string (4)"


###############################################################################
# Test handling of DIMENSION anonymous block insertion (#7120)


def test_ogr_dxf_46():

    ds = ogr.Open("data/dxf/dimension.dxf")
    lyr = ds.GetLayer(0)

    # Extension lines
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "LINESTRING Z (320000.0 5820010.0625 0,320000.0 5820010.43087258 0)"
    )
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "LINESTRING Z (320010.0 5820010.0625 0,320010.0 5820010.43087258 0)"
    )

    # Dimension arrow lines
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING Z (320000.18 5820010.25087258 0,320004.475225102 5820010.25087258 0)",
    )
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING Z (320009.82 5820010.25087258 0,320005.524774898 5820010.25087258 0)",
    )

    # Arrowheads
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((320000.18 5820010.28087259,320000.18 5820010.22087258,320000.0 5820010.25087258,320000.18 5820010.28087259))",
    )
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((320009.82 5820010.28087259,320009.82 5820010.22087258,320010.0 5820010.25087258,320009.82 5820010.28087259))",
    )

    # Text
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT Z (320004.537844475 5820010.16240737 0)")
    assert (
        f.GetStyleString() == 'LABEL(f:"Arial",t:"10.0000",p:1,s:0.18g,c:#000000)'
    ), "Wrong style string on DIMENSION text from block"


###############################################################################
# Test handling of DIMENSION fallback when there is no anonymous block (#7120)


def test_ogr_dxf_47():

    ds = ogr.Open("data/dxf/dimension-entities-only.dxf")
    lyr = ds.GetLayer(0)

    # Basic DIMENSION inheriting default styling

    # Dimension line and extension lines
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTILINESTRING ((320010.0 5820010.25087258,320000.0 5820010.25087258),(320010.0 5820010.0625,320010.0 5820010.43087258),(320000.0 5820010.0625,320000.0 5820010.43087258))",
    )

    # Arrowheads
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((320009.82 5820010.28087259 0,320010.0 5820010.25087258 0,320009.82 5820010.22087258 0,320009.82 5820010.28087259 0))",
    )
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((320000.18 5820010.22087258 0,320000.0 5820010.25087258 0,320000.18 5820010.28087259 0,320000.18 5820010.22087258 0))",
    )

    # Text
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT (320005.0 5820010.25087258)")
    assert (
        f.GetStyleString() == 'LABEL(f:"Arial",t:"10.0000",p:11,s:0.18g,c:#000000)'
    ), "Wrong style string on first DIMENSION text"

    # DIMENSION with style overrides

    # Dimension line
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "MULTILINESTRING ((320005 5820005,320000 5820010))"
    )

    # Arrowheads
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((320004.116116524 5820006.23743687 0,320005 5820005 0,320003.762563133 5820005.88388348 0,320004.116116524 5820006.23743687 0))",
    )
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((320000.883883476 5820008.76256313 0,320000 5820010 0,320001.237436867 5820009.11611652 0,320000.883883476 5820008.76256313 0))",
    )

    # Text
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT (320002.5 5820007.5)")
    assert (
        f.GetStyleString() == 'LABEL(f:"Arial",t:"7.1",p:11,a:-45,s:0.48g,c:#000000)'
    ), "Wrong style string on second DIMENSION text"

    # DIMENSION inheriting styles from a custom DIMSTYLE

    # Dimension line
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTILINESTRING ((320000.0 5820001.5,320005.0 5820001.5),(320000.0 5820002.4,320000 5820001),(320005.0 5820002.4,320005 5820001))",
    )

    # Arrowheads
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((320000.18 5820001.47 0,320000.0 5820001.5 0,320000.18 5820001.53 0,320000.18 5820001.47 0))",
    )
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((320004.82 5820001.53 0,320005.0 5820001.5 0,320004.82 5820001.47 0,320004.82 5820001.53 0))",
    )

    # Text
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(f, "POINT (320001.5 5820001.5)")
    assert (
        f.GetStyleString()
        == 'LABEL(f:"Arial",t:"±2 3\n\\P4 5.0000",p:11,s:0.18g,c:#000000)'
    ), "Wrong style string on third DIMENSION text"


###############################################################################
# Rudimentary test of ByLayer and ByBlock color values (#7130)


def test_ogr_dxf_48():

    with gdal.config_option("DXF_MERGE_BLOCK_GEOMETRIES", "FALSE"):
        ds = ogr.Open("data/dxf/byblock-bylayer.dxf")

    lyr = ds.GetLayer(0)

    # First insert an anonymous dimension block (this is NOT a test of our
    # basic "dimension" renderer)

    # The dimension extension lines are ByBlock; the INSERT is magenta
    f = lyr.GetFeature(0)
    assert (
        f.GetStyleString() == 'PEN(c:#ff00ff,p:"1.5875g 1.5875g")'
    ), "Wrong style string on feature 0"

    # The dimension line is set directly to blue
    f = lyr.GetFeature(2)
    assert f.GetStyleString() == "PEN(c:#0000ff)", "Wrong style string on feature 2"

    # The first arrowhead is a custom block; the SOLID in this block is
    # colored ByLayer; the layer the block is inserted on (_K_POINTS)
    # is colored red
    f = lyr.GetFeature(4)
    assert f.GetStyleString() == "BRUSH(fc:#ff0000)", "Wrong style string on feature 4"

    # The first arrowhead block also contains a line colored ByBlock.
    # The arrowhead INSERT is blue, so the line should be blue.
    # Because this INSERT is within another block, we need to make
    # sure the ByBlock colouring isn't handled again for the outer
    # block, which is magenta.
    f = lyr.GetFeature(5)
    assert f.GetStyleString() == "PEN(c:#0000ff)", "Wrong style string on feature 5"

    # The second arrowhead, like the dimension line, is set directly
    # to blue
    f = lyr.GetFeature(6)
    assert f.GetStyleString() == "BRUSH(fc:#0000ff)", "Wrong style string on feature 6"

    # Like the dimension extension lines, the text is ByBlock (#7099)
    f = lyr.GetFeature(7)
    assert (
        f.GetStyleString() == 'LABEL(f:"Arial",t:"10.141 (2C)",s:0.4g,p:5,c:#ff00ff)'
    ), "Wrong style string on feature 7"

    # ByLayer feature in block
    f = lyr.GetFeature(11)
    assert f.GetStyleString() == "PEN(c:#ff0000)", "Wrong style string on feature 11"

    # Since the INSERT is in PaperSpace, this feature should be too
    assert f.GetField("PaperSpace") == 1, "Wrong PaperSpace on feature 11"

    # ByBlock feature in block
    f = lyr.GetFeature(12)
    assert f.GetStyleString() == "PEN(c:#a552a5)", "Wrong style string on feature 12"

    # ByLayer feature inserted via an INSERT on yellow layer in block
    # inserted via an INSERT on red layer: should be yellow
    f = lyr.GetFeature(13)
    assert f.GetStyleString() == "PEN(c:#ffff00)", "Wrong style string on feature 13"

    # ByBlock feature inserted via a ByBlock INSERT in block inserted
    # via a color213 INSERT: should be color213
    f = lyr.GetFeature(14)
    assert f.GetStyleString() == "PEN(c:#a552a5)", "Wrong style string on feature 14"

    # ByBlock entities directly on the canvas show up as black
    f = lyr.GetFeature(15)
    assert f.GetStyleString() == "PEN(c:#000000)", "Wrong style string on feature 15"

    # ByBlock feature inserted via a true-color INSERT
    f = lyr.GetFeature(16)
    assert f.GetStyleString() == "PEN(c:#18dce1)", "Wrong style string on feature 16"

    # True-color feature in block
    f = lyr.GetFeature(17)
    assert f.GetStyleString() == "PEN(c:#5e089b)", "Wrong style string on feature 17"

    # ByBlock feature inserted via a ByLayer INSERT on true-color layer
    f = lyr.GetFeature(18)
    assert f.GetStyleString() == "PEN(c:#8ef19c)", "Wrong style string on feature 18"


###############################################################################
# Test block attributes (ATTRIB entities) (#7139)


def test_ogr_dxf_49():

    # Inline blocks mode
    ds = ogr.Open("data/dxf/attrib.dxf")
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 8, (
        "Wrong feature count, got %d" % lyr.GetFeatureCount()
    )

    f = lyr.GetFeature(1)
    assert (
        f.GetStyleString()
        == 'LABEL(f:"Arial",t:"Constant attribute",p:2,a:8,s:8g,dx:-1g,dy:8g,c:#000000)'
    ), "Wrong style string on constant attribute on first INSERT"

    f = lyr.GetFeature(2)
    assert (
        f.GetField("Text") == "super test"
    ), "Wrong Text value on first ATTRIB on first INSERT"
    assert (
        f.GetStyleString()
        == 'LABEL(f:"Arial",t:"super test",p:2,s:8g,w:234.6,dx:30.293g,c:#ff0000)'
    ), "Wrong style string on first ATTRIB on first INSERT"

    f = lyr.GetFeature(5)
    geom = f.GetGeometryRef()
    assert geom.GetGeometryType() == ogr.wkbLineString25D, "Expected LINESTRING Z"

    f = lyr.GetFeature(7)
    assert f.GetField("Text") == "", "Wrong Text value on ATTRIB on second INSERT"

    # No inlining
    with gdal.config_option("DXF_INLINE_BLOCKS", "FALSE"):
        ds = ogr.Open("data/dxf/attrib.dxf")

    lyr = ds.GetLayerByName("entities")

    f = lyr.GetFeature(0)
    assert f.GetField("BlockAttributes") == [
        "MYATT1 super test",
        "MYATTMULTI_001 Corps",
        "MYATTMULTI_002 plpl",
    ], "Wrong BlockAttributes value on first INSERT"

    f = lyr.GetFeature(1)
    assert f.GetField("BlockAttributes") == [
        "MYATTMULTI "
    ], "Wrong BlockAttributes value on second INSERT"

    lyr = ds.GetLayerByName("blocks")

    assert lyr.GetFeatureCount() == 4

    f = lyr.GetFeature(1)
    assert (
        f.GetField("AttributeTag") == "MYATT1"
    ), "Wrong AttributeTag value on first ATTDEF"

    f = lyr.GetFeature(2)
    assert (
        f.GetField("AttributeTag") == None
    ), "Wrong AttributeTag value on second (constant) ATTDEF"
    assert (
        f.GetField("Text") == "Constant attribute"
    ), "Wrong Text value on second (constant) ATTDEF"

    f = lyr.GetFeature(3)
    assert (
        f.GetField("AttributeTag") == "MYATTMULTI"
    ), "Wrong AttributeTag value on third ATTDEF"


###############################################################################
# Test blocks that insert blocks that themselves have attributes


def test_ogr_dxf_49a():

    with gdal.config_option("DXF_MERGE_BLOCK_GEOMETRIES", "FALSE"):
        ds = ogr.Open("data/dxf/attrib-nested.dxf")

    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1

    f = lyr.GetFeature(0)
    ogrtest.check_feature_geometry(f, "POINT Z (0 0 0)")
    assert (
        f.GetStyleString()
        == 'LABEL(f:"Arial",t:"Gamma Goochee",p:1,s:0.4g,w:100,c:#000000)'
    )


###############################################################################
# Test extended text styling (#7151) and additional ByBlock/ByLayer tests (#7130)


def test_ogr_dxf_50():

    with gdal.config_option("DXF_MERGE_BLOCK_GEOMETRIES", "FALSE"):
        ds = ogr.Open("data/dxf/text-fancy.dxf")

    lyr = ds.GetLayer(0)

    # Text in Times New Roman bold italic, stretched 190%, color ByLayer
    # inside block inserted on a blue layer
    f = lyr.GetFeature(0)
    assert (
        f.GetStyleString()
        == 'LABEL(f:"Times New Roman",bo:1,it:1,t:"Some nice text",p:5,s:10g,w:190,dx:84.3151g,dy:4.88825g,c:#0000ff)'
    ), "Wrong style string on feature 0"

    # Polyline, color and linetype ByBlock inside block with red color
    # and ByLayer linetype inserted on a layer with DASHED2 linetype
    f = lyr.GetFeature(1)
    assert (
        f.GetStyleString() == 'PEN(c:#ff0000,w:2.1g,p:"2.5g 1.25g")'
    ), "Wrong style string on feature 1"

    # Make sure TEXT objects don't inherit anything other than font name,
    # bold and italic from their parent STYLE
    f = lyr.GetFeature(2)
    assert (
        f.GetStyleString()
        == 'LABEL(f:"Times New Roman",bo:1,it:1,t:"Good text",p:1,s:5g,c:#000000)'
    ), "Wrong style string on feature 2"

    # Polyline, color ByBlock, inside block inserted on a blue layer
    f = lyr.GetFeature(3)
    assert (
        f.GetStyleString() == "PEN(c:#0000ff,w:2.1g)"
    ), "Wrong style string on feature 3"

    # MTEXT stretched 250%, color ByLayer inside block inserted on a blue layer
    f = lyr.GetFeature(4)
    assert (
        f.GetStyleString()
        == 'LABEL(f:"Times New Roman",bo:1,it:1,t:"Some nice MTEXT",s:10g,w:250,p:8,c:#0000ff)'
    ), "Wrong style string on feature 4"

    # Individually invisible object should be invisible
    f = lyr.GetFeature(5)
    assert (
        f.GetStyleString()
        == 'LABEL(f:"Times New Roman",bo:1,it:1,t:"Invisible text",p:1,s:5g,c:#00000000)'
    ), "Wrong style string on feature 5"


###############################################################################
# Test transformation of text inside blocks (ACAdjustText function)


def test_ogr_dxf_51():

    ds = ogr.Open("data/dxf/text-block-transform.dxf")

    lyr = ds.GetLayer(0)

    wanted_style = [
        "a:330",
        "c:#000000",
        "dx:1.96672g",
        "dy:-1.13549g",
        'f:"Arial"',
        "p:2",
        "s:3g",
        't:"some text"',
        "w:25",
    ]

    # Three text features, all with the same effective geometry and style
    for x in range(3):
        f = lyr.GetNextFeature()

        ogrtest.check_feature_geometry(
            f, "POINT Z (2.83231568033604 5.98356393304499 0)", context=f"feature {x}"
        )

        if sorted(f.GetStyleString()[6:-1].split(",")) != wanted_style:
            f.DumpReadable()
            pytest.fail("Wrong style string on feature %d" % x)


###############################################################################
# Test HELIX, TRACE, HATCH with spline boundary, MLINE, and INSERT with rows/columns


def test_ogr_dxf_52():

    ds = ogr.Open("data/dxf/additional-entities.dxf")
    lyr = ds.GetLayer(0)

    # HELIX
    f = lyr.GetNextFeature()
    assert (
        f.GetField("SubClasses") == "AcDbEntity:AcDbSpline:AcDbHelix"
    ), "Wrong SubClasses on HELIX"
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING Z (150 120 0,149.345876458438 119.778561209114 0.210526315789474,148.706627788813 119.535836602547 0.421052631578947,148.082773142501 119.272634882071 0.631578947368421,147.474831670876 118.989764749454 0.842105263157894,146.883322525316 118.688034906466 1.05263157894737,146.308764857195 118.368254054878 1.26315789473684,145.75167781789 118.03123089646 1.47368421052632,145.212580558776 117.677774132981 1.68421052631579,144.691992231228 117.308692466212 1.89473684210526,144.190431986623 116.924794597921 2.10526315789474,143.708418976337 116.52688922988 2.31578947368421,143.246472351745 116.115785063858 2.52631578947368,142.805226682224 115.692350328976 2.73684210526316,142.385468357145 115.257680939095 2.94736842105263,141.987209053809 114.8126724387 3.1578947368421,141.610382578047 114.358190780749 3.36842105263158,141.254922735687 113.895101918197 3.57894736842105,140.920763332559 113.424271804003 3.78947368421053,140.607838174492 112.946566391121 4,140.316081067316 112.46285163251 4.21052631578947,140.04542581686 111.973993481125 4.42105263157895,139.795806228954 111.480857889924 4.63157894736842,139.567156109426 110.984310811863 4.8421052631579,139.359409264107 110.4852181999 5.05263157894737,139.172499498825 109.98444600699 5.26315789473684,139.00636061941 109.482860186091 5.47368421052632,138.860926431692 108.981326690159 5.68421052631579,138.7361307415 108.480711472151 5.89473684210526,138.631907354662 107.981880485024 6.10526315789474,138.54819007701 107.485699681734 6.31578947368421,138.484912714371 106.993035015239 6.52631578947369,138.442009072576 106.504752438495 6.73684210526316,138.419412957453 106.021717904458 6.94736842105264,138.41678542913 105.544991394258 7.15789473684211,138.433333223564 105.075482938615 7.36842105263158,138.468434844937 104.613670677827 7.57894736842106,138.521471087835 104.160029888371 7.78947368421053,138.59182274684 103.715035846723 8.00000000000001,138.678870616536 103.27916382936 8.21052631578948,138.781995491508 102.852889112758 8.42105263157896,138.900578166339 102.436686973394 8.63157894736843,139.033999435614 102.031032687745 8.8421052631579,139.181640093916 101.636401532287 9.05263157894738,139.342880935829 101.253268783496 9.26315789473685,139.517102755937 100.882109717849 9.47368421052633,139.703686348824 100.523399611823 9.6842105263158,139.902012509075 100.177613741895 9.89473684210528,140.111462031272 99.8452273845396 10.1052631578948,140.33141571 99.5267158162348 10.3157894736842,140.561254339843 99.2225543134567 10.5263157894737,140.800358715385 98.933218152682 10.7368421052632,141.04810963121 98.6591826103871 10.9473684210526,141.303887881901 98.4009229630486 11.1578947368421,141.567055122443 98.1589144355755 11.3684210526316,141.836465751876 97.9334729764931 11.5789473684211,142.111203876514 97.7245055448538 11.7894736842105,142.390570491032 97.5318567084626 12,142.673866590107 97.3553710351246 12.2105263157895,142.960393168413 97.194893092645 12.421052631579,143.249451220625 97.0502674488286 12.6315789473684,143.54034174142 96.9213386714808 12.8421052631579,143.832365725473 96.8079513284064 13.0526315789474,144.124824167458 96.7099499874106 13.2631578947369,144.417018062052 96.6271792162984 13.4736842105263,144.708248403929 96.5594835828749 13.6842105263158,144.997816187765 96.5067076549452 13.8947368421053,145.285022408236 96.4686960003143 14.1052631578948,145.569168060017 96.4452931867873 14.3157894736842,145.849554137782 96.4363437821692 14.5263157894737,146.125481636209 96.4416923542652 14.7368421052632,146.396251549971 96.4611834708803 14.9473684210527,146.661164873745 96.4946616998195 15.1578947368421,146.919522602205 96.5419716088879 15.3684210526316,147.170625730027 96.6029577658907 15.5789473684211,147.413708536343 96.6773765373194 15.7894736842105,147.64790100184 96.7644518356677 16,147.872845806317 96.8635266377703 16.2105263157895,148.088244263586 96.9739879715066 16.421052631579,148.293797687463 97.0952228647559 16.6315789473684,148.48920739176 97.2266183453974 16.8421052631579,148.674174690292 97.3675614413103 17.0526315789474,148.848400896871 97.5174391803741 17.2631578947369,149.011587325312 97.675638590468 17.4736842105263,149.163435289429 97.8415466994713 17.6842105263158,149.303646103034 98.0145505352631 17.8947368421053,149.431921079943 98.194037125723 18.1052631578948,149.547961533967 98.37939349873 18.3157894736842,149.651468778922 98.5700066821635 18.5263157894737,149.742144128621 98.7652637039028 18.7368421052632,149.819688896877 98.9645515918272 18.9473684210527,149.883804397505 99.1672573738159 19.1578947368421,149.934191944317 99.3727680777483 19.3684210526316,149.970552851128 99.5804707315036 19.5789473684211,149.992588431751 99.7897523629611 19.7894736842106,150 100 20)",
        context="HELIX",
    )

    # TRACE
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((150.0 120.5,150.0 119.5,200.0 119.5,200.0 120.5,150.0 120.5))",
        context="TRACE",
    )

    # HATCH with a spline boundary path (and OCS as well, just for fun)
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON Z ((47.6969600708475 60.0 15,47.6969600708475 0.0 15,46.1103652823066 -0.466958240734954 14.5010390223444,44.5309994192688 -0.919910449553494 14.0043514365868,42.9660914072371 -1.34485059453921 13.5122106346236,41.4228701717145 -1.72777264377568 13.0268900083519,39.9085646382042 -2.0546705653465 12.5506629496691,38.4304037322091 -2.31153832733525 12.0858028504722,36.9956163792324 -2.48436989782552 11.6345831026584,35.6114315047771 -2.55915924490089 11.1992770981251,34.2850780343463 -2.52190033664495 10.7821582287693,33.0237848934429 -2.3585871411413 10.3854998864882,31.8347810075701 -2.0552136264735 10.011575463179,30.725295302231 -1.59777376072516 9.66265835073903,29.7025567029285 -0.972261511979859 9.34102194106535,28.7737941351658 -0.164670848321179 9.04893962605519,27.9445456607789 0.835923776643351 8.78815304558283,27.2086691364137 2.01916842728349 8.55673058492536,26.5550905172208 3.36572402537053 8.35118961809371,25.9727183005027 4.85621968724478 8.1680420288596,25.450460983562 6.47128452924656 8.00379970099481,24.9772270637013 8.19154766771616 7.85497451827107,24.5419250382231 9.99763821899391 7.71807836446012,24.1334634044299 11.8701852994201 7.58962312333373,23.7407506596245 13.7898180253351 7.46612067866363,23.3526953011092 15.7371655130791 7.34408291422158,22.9582058261868 17.6928568789925 7.22002171377933,22.5461907321598 19.6375212394157 7.09044896110861,22.1055585163308 21.5517877106888 6.95187653998118,21.6252176760022 23.4162854091522 6.80081633416879,21.0940767084768 25.2116434511463 6.63378022744318,20.501044111057 26.9184909530113 6.44728010357611,19.8350283810455 28.5174570310876 6.23782784633932,19.0849380157448 29.9891708017154 6.00193533950455,18.2425220975445 31.3190096857923 5.73700778952582,17.3111586046656 32.5117898949509 5.44410752140743,16.2972009340528 33.5773013324584 5.12523258605305,15.2070024839932 34.5253339038266 4.7823810347885,14.046916652774 35.3656775145671 4.41755091893963,12.8232968386826 36.1081220701913 4.0327402898323,11.5424964400062 36.7624574762111 3.62994719879235,10.2108688550319 37.338473638138 3.21116969714562,8.834767482047 37.8459604614835 2.77840583621797,7.42054571933875 38.2947078517594 2.33365366733523,5.97455696519436 38.6945057144772 1.87891124182326,4.50315461790106 39.0551439551486 1.41617661100791,3.01269207574607 39.3864124792851 0.947447826215011,1.50952273701662 39.6981011923983 0.474722938770421,0 40 0,47.6969600708475 60.0 15))",
        context="HATCH 1",
    )

    # Another HATCH with a spline boundary path
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "POLYGON ((77.2409948093467 172.430072843974,75.261065049518 171.643815908613,73.2981627172517 170.696359298076,71.3875263981659 169.604375316261,69.5631545040892 168.388782317518,67.8563613292361 167.073645833116,66.2946346031063 165.684923719356,64.9008927779373 164.249191641798,63.6931834376534 162.792474868402,62.6848095284188 161.339284018754,61.8848281190297 159.911914493846,61.298843085132 158.530030785262,60.9300095654357 157.21052494952,60.7801817312349 155.967617443343,60.8511622391671 154.813159397702,61.1453506194465 153.756315627339,61.664528568996 152.803726831033,62.4105189258065 151.964301284269,63.3843801494824 151.249807149788,64.5850174245011 150.674379748575,66.0076885629501 150.253830592488,67.6424701411086 150.004704104407,69.4728079995063 149.943055679116,71.4743351228068 150.082969908572,73.6141841756299 150.434899728198,75.8510314766708 151.003978292885,78.1360638882735 151.788519596984,80.41494916904 152.778958906121,82.6307223520159 153.957467932202,84.7273090430607 155.298400731333,86.6532441052495 156.769592674546,88.3650659486779 158.334376895018,89.8299059955281 159.954044613131,91.0272387483612 161.589690168686,91.957368361883 163.189390065386,92.6421956980388 164.701128484532,93.1110835351921 166.094608264522,93.3942229550992 167.356341648662,93.5189493216415 168.483994478151,93.5079031853763 169.481912548033,93.3782987206629 170.357905428604,93.1417233193846 171.121075154443,92.8040593119231 171.78040251468,92.3652285565357 172.343822183981,91.8186752047856 172.817679470437,91.1571081487304 173.20986823606,90.3766844995721 173.528608818345,89.4716123962251 173.777329886712,88.4346297478537 173.955199520934,87.2580461173156 174.057607166253,85.9350751682361 174.076420167619,84.4616595153474 174.000186427787,82.8389797200317 173.814465060529,81.0767853391384 173.502507153586,79.1975328516125 173.04655812613,77.2409948093467 172.430072843974))",
        context="HATCH 2",
    )

    # Three MLINE objects
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTILINESTRING Z ((-3.92232270276368 270.388386486182 0,44.2014737139232 260.763627202844 0),(0 290 0,50 280 0),(50 280 0,54.2440667916678 280.848813358334 0),(66.6666666666666 283.333333333333 0,87.2937093466817 287.458741869336 0),(55.335512192016 260.671024384032 0,83.0445264186877 266.212827229366 0),(97.9166666666667 289.583333333333 0,150 300 0),(93.6674837386727 268.337418693363 0,122.93205511402 274.190332968433 0),(150 300 0,140 260 0),(122.93205511402 274.190332968433 0,120.597149997093 264.850712500727 0))",
        context="MLINE 1",
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTILINESTRING Z ((70 290 0,50 250 0),(61.0557280900008 294.472135955 0,41.0557280900008 254.472135955 0))",
        context="MLINE 2",
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "MULTILINESTRING Z ((100 300 0,97.9166666666667 289.583333333333 0),(95.7739043877141 304.364619506534 0,92.6051880066742 288.521037601335 0),(91.5478087754281 308.729239013068 0,87.2937093466817 287.458741869336 0),(93.6674837386727 268.337418693363 0,90 250 0),(88.3560050786802 267.275122961365 0,83.7111464107331 244.050829621629 0),(83.0445264186877 266.212827229366 0,77.4222928214662 238.101659243259 0),(90 250 0,160 260 0),(83.7111464107331 244.050829621629 0,165.0 255.663522991525 0),(77.4222928214662 238.101659243259 0,170.0 251.327045983049 0),(160 260 0,160 310 0),(165.0 255.663522991525 0,165.0 315.902302108582 0),(170.0 251.327045983049 0,170.0 321.804604217164 0),(160 310 0,100 300 0),(165.0 315.902302108582 0,95.7739043877141 304.364619506534 0),(170.0 321.804604217164 0,91.5478087754281 308.729239013068 0))",
        context="MLINE 3",
    )

    # INSERT with rows/columns (MInsert)
    minsert_attrib_style = (
        'LABEL(f:"Arial",t:"N",p:5,a:13,s:8g,w:120,dx:2.21818g,dy:4.61732g,c:#000000)'
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING (57.7504894565613 50.7437006478524,69.4429302339842 53.4431132999787,71.6924407774228 43.6994126521264,60 41,57.7504894565613 50.7437006478524)",
        context="INSERT polyline 1",
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "POINT Z (62.5032851270548 42.604233016948 0)", context="INSERT attribute 1"
    )
    assert (
        f.GetStyleString() == minsert_attrib_style
    ), "Wrong style on INSERT attribute 1"

    for _ in range(2):
        f = lyr.GetNextFeature()

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING (116.212693343675 64.2407639084843,127.905134121098 66.9401765606106,130.154644664537 57.1964759127583,118.462203887114 54.4970632606319,116.212693343675 64.2407639084843)",
        context="INSERT polyline 3",
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "POINT Z (120.965489014169 56.1012962775799 0)", context="INSERT attribute 3"
    )
    assert (
        f.GetStyleString() == minsert_attrib_style
    ), "Wrong style on INSERT attribute 3"

    for _ in range(8):
        f = lyr.GetNextFeature()

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING (140.944774200355 90.4766968345049,152.637214977778 93.1761094866313,154.886725521217 83.4324088387789,143.194284743794 80.7329961866526,140.944774200355 90.4766968345049)",
        context="INSERT polyline 8",
    )

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "POINT Z (145.697569870849 82.3372292036006 0)", context="INSERT attribute 8"
    )
    assert (
        f.GetStyleString() == minsert_attrib_style
    ), "Wrong style on INSERT attribute 8"

    # Also throw in a test of a weird SPLINE generated by a certain CAD package
    # with a knot vector that does not start at zero
    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f,
        "LINESTRING Z (0 20 0,0.513272464826192 19.8251653183892 0,1.00815682586353 19.629626397244 0,1.48499546839613 19.4132825350102 0,1.94413077770813 19.1760330301337 0,2.38590513908363 18.9177771810603 0,2.81066093780676 18.6384142862359 0,3.21874055916165 18.3378436441062 0,3.61048638843241 18.0159645531172 0,3.98624081090316 17.6726763117148 0,4.34634621185803 17.3078782183446 0,4.69114497658114 16.9214695714527 0,5.02097949035661 16.5133496694848 0,5.33619213846856 16.0834178108867 0,5.63712530620111 15.6315732941045 0,5.92412137883838 15.1577154175837 0,6.1975227416645 14.6617434797705 0,6.45767177996359 14.1435567791104 0,6.70491087901976 13.6030546140496 0,6.93958242411715 13.0401362830336 0,7.16202880053986 12.4547010845085 0,7.37259239357203 11.8466483169201 0,7.57161558849776 11.2158772787141 0,7.7594407706012 10.5622872683365 0,7.93641032516645 9.88577758423314 0,8.10286663747763 9.18624752484979 0,8.25915209281888 8.46359638863234 0,8.4056090764743 7.71772347402662 0,8.54257997372803 6.94852807947849 0,8.67040716986418 6.1559095034338 0,8.78943305016688 5.33976704433838 0,8.9 4.5 0)",
        context="SPLINE",
    )


###############################################################################
# Test block base points


def test_ogr_dxf_53():

    ds = ogr.Open("data/dxf/block-basepoint.dxf")
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        f, "MULTILINESTRING Z ((290 160 0,310 140 0),(310 160 0,290 140 0))"
    )


###############################################################################
# Test frozen and off layers


def test_ogr_dxf_54(tmp_vsimem):

    # Note about the test file frozen-off.dxf:
    #
    # There are five layers in the file: the default layer 0 and four other
    # layers named according to their visibility (ON/OFF) and frozenness
    # (THAW/FREEZE). Entities are assigned to layers according to the quadrant
    # of the Cartesian plane they lie within, according to the following plan
    # where + represents (0,0):
    #
    #     OFFTHAW  |  ONTHAW
    #   -----------+----------
    #    OFFFREEZE | ONFREEZE
    #
    # This arrangement applies not only to the entities on the drawing itself,
    # but also to the entities on each block within the drawing.
    #
    # The circle/polyline entities at the center of the blocks are on layer 0.

    with gdal.config_option("DXF_MERGE_BLOCK_GEOMETRIES", "FALSE"):
        ds = ogr.Open("data/dxf/frozen-off.dxf")
    lyr = ds.GetLayer(0)

    # Features should be visible/hidden in the following order:
    featureVisibility = ".hhh..hhh..hhhhhhhhhhhhhh.hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh.hhh..hhhhhhhhhhhhhh.hhh"

    for number, h in enumerate(featureVisibility):
        f = lyr.GetNextFeature()
        isFeatureVisible = (
            "#000000)" in f.GetStyleString() or "#ff0000)" in f.GetStyleString()
        )
        if isFeatureVisible == (h == "h"):
            f.DumpReadable()
            pytest.fail(
                "Wrong visibility on feature %d (testing with layer 0 thawed)" % number
            )

    # Rewrite the test file, this time with layer 0 set as frozen
    with open("data/dxf/frozen-off.dxf", "r") as file:
        gdal.FileFromMemBuffer(
            tmp_vsimem / "frozen-off-with-layer0-frozen.dxf",
            file.read().replace(
                "0\nLAYER\n  2\n0\n 70\n     0", "0\nLAYER\n  2\n0\n 70\n     1"
            ),
        )

    with gdal.config_option("DXF_MERGE_BLOCK_GEOMETRIES", "FALSE"):
        ds = ogr.Open(
            tmp_vsimem / "frozen-off-with-layer0-frozen.dxf",
        )
    lyr = ds.GetLayer(0)

    # Repeat test - outcome should be the same
    for number, h in enumerate(featureVisibility):
        f = lyr.GetNextFeature()
        isFeatureVisible = (
            "#000000)" in f.GetStyleString() or "#ff0000)" in f.GetStyleString()
        )
        if isFeatureVisible == (h == "h"):
            f.DumpReadable()
            pytest.fail(
                "Wrong visibility on feature %d (testing with layer 0 frozen)" % number
            )


###############################################################################
# Comprehensive test of ByBlock and ByLayer color handling


def test_ogr_dxf_54a(tmp_vsimem):

    # Note about the test file byblock-bylayer-new.dxf:
    #
    # There are three layers in the file, layer 0 (black), MYLAYERRED (red)
    # and MYLAYERBLUE (blue).
    # The drawing contains two levels of nested blocks:
    #    drawing -> DEMOBLOCK -> DEMOBLOCKWITHSUB
    # Geometrically speaking, the drawing itself, and each block within
    # the drawing, is divided as follows:
    #
    #      ByBlock   |     Set*    |   ByLayer
    #      layer 0   |   layer 0   |   layer 0
    #   -------------+-------------+-------------
    #      ByBlock   |     Set*    |   ByLayer
    #    MYLAYERRED  | MYLAYERRED  | MYLAYERRED
    #   -------------+-------------+-------------
    #      ByBlock   |     Set*    |   ByLayer
    #    MYLAYERBLUE | MYLAYERBLUE | MYLAYERBLUE
    #
    # * "Set" indicates that the color is set directly (group code 62).
    #   On the drawing the color used is green. On DEMOBLOCK the color
    #   is cyan. On DEMOBLOCKWITHSUB the color is yellow.

    with gdal.config_option("DXF_MERGE_BLOCK_GEOMETRIES", "FALSE"):
        ds = ogr.Open("data/dxf/byblock-bylayer-new.dxf")
    lyr = ds.GetLayer(0)

    # Features should be colored in the following order:
    featureColors = (
        "77727127527472412452477271275271121121521412412452"  # 0
        + "47127127527552512552545241245247527127527377271275"  #
        + "27472412452437231235231121121521412412452431231235"  # 100
        + "23552512552545241245243523123523777271275274724124"  #
        + "52477271275271121121521412412452471271275275525125"  # 200
        + "52545241245247527127527111211215214124124524112112"  #
        + "15211121121521412412452411211215215525125525452412"  # 300
        + "45241521121521311211215214124124524312312352311211"  #
        + "21521412412452431231235235525125525452412452435231"  # 400
        + "23523711211215214124124524712712752711211215214124"  #
        + "12452471271275275525125525452412452475271275275552"  # 500
        + "51255254524124524552512552511211215214124124524512"  #
        + "51255255525125525452412452455251255253552512552545"  # 600
        + "24124524352312352311211215214124124524312312352355"  #
        + "25125525452412452435231235237552512552545241245247"  # 700
        + "52712752711211215214124124524712712752755251255254"  #
        + "5241245247527127527"  # 800
    )

    featureColorDictionary = {
        "#ff0000": "1",  # red
        "#ffff00": "2",  # yellow
        "#00ff00": "3",  # green
        "#00ffff": "4",  # cyan
        "#0000ff": "5",  # blue
        "#000000": "7",  # black
    }

    for number, expectedColorCode in enumerate(featureColors):
        f = lyr.GetNextFeature()
        actualColor = re.search("c:(#......)", f.GetStyleString()).group(1)

        if actualColor not in featureColorDictionary:
            f.DumpReadable()
            pytest.fail("Unknown color %s on feature %d" % (actualColor, number))

        actualColorCode = featureColorDictionary[actualColor]
        if actualColorCode != expectedColorCode:
            f.DumpReadable()
            pytest.fail(
                "Wrong color on feature %d (got %s, expected %s)"
                % (
                    number,
                    actualColor,
                    [
                        hex
                        for hex, code in featureColorDictionary.items()
                        if code == expectedColorCode
                    ][0],
                )
            )


###############################################################################
# Test hidden objects in blocks


@pytest.mark.parametrize("use_config_option", [True, False])
def test_ogr_dxf_55(use_config_option):

    if use_config_option:
        with gdaltest.config_option("DXF_MERGE_BLOCK_GEOMETRIES", "FALSE"):
            ds = ogr.Open("data/dxf/block-hidden-entities.dxf")
    else:
        ds = gdal.OpenEx(
            "data/dxf/block-hidden-entities.dxf",
            open_options={"MERGE_BLOCK_GEOMETRIES": False},
        )
    lyr = ds.GetLayer(0)

    assert lyr.GetFeatureCount() == 6

    # Red features should be hidden, black features should be visible
    for number, f in enumerate(lyr):
        assert "#ff000000)" in f.GetStyleString() or "#000000)" in f.GetStyleString(), (
            "Wrong visibility on feature %d" % number
        )


###############################################################################
def test_ogr_dxf_insert_too_many_errors():

    with gdal.quiet_errors():
        ogr.Open("data/dxf/insert-too-many-errors.dxf")


###############################################################################


def test_ogr_dxf_write_geometry_collection_of_unsupported_type(tmp_vsimem):

    tmpfile = tmp_vsimem / "ogr_dxf_write_geometry_collection_of_unsupported_type.dxf"
    ds = ogr.GetDriverByName("DXF").CreateDataSource(tmpfile)
    lyr = ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt("GEOMETRYCOLLECTION(TIN EMPTY)"))
    with gdal.quiet_errors():
        ret = lyr.CreateFeature(f)
    assert ret != 0
    ds = None


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/1969
# with a SPLINE whose first knot is a very close to zero negative value.


def test_ogr_dxf_very_close_neg_to_zero_knot():

    ds = ogr.Open("data/dxf/spline_with_very_close_neg_to_zero_knot.dxf")
    lyr = ds.GetLayer(0)

    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    extent = g.GetEnvelope()
    assert extent == pytest.approx(
        (163.0306017054786, 166.6530957511469, 78.40469559017359, 81.82569418640966),
        abs=1e-5,
    )


###############################################################################


def test_ogr_dxf_polygon_3D(tmp_vsimem):

    tmpfile = tmp_vsimem / "test_ogr_dxf_polygon_3D.dxf"
    ds = ogr.GetDriverByName("DXF").CreateDataSource(tmpfile)
    lyr = ds.CreateLayer("test")
    f = ogr.Feature(lyr.GetLayerDefn())
    g = ogr.CreateGeometryFromWkt("POLYGON((0 0 10,0 1 10,1 1 10,0 0 10))")
    f.SetGeometry(g)
    lyr.CreateFeature(f)
    ds = None
    ds = ogr.Open(tmpfile)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    got_g = f.GetGeometryRef()
    assert got_g.Equals(g)


###############################################################################


def test_ogr_dxf_read_broken_file_1():
    """Test that we don't crash"""

    with gdal.quiet_errors():
        ds = ogr.Open(
            "data/dxf/clusterfuzz-testcase-minimized-dxf_fuzzer-5400376672124928.dxf"
        )
        lyr = ds.GetLayer(0)
        for f in lyr:
            pass


###############################################################################


def test_ogr_dxf_read_broken_file_2():
    """Test that we don't crash"""

    with gdal.quiet_errors():
        ds = ogr.Open(
            "data/dxf/clusterfuzz-testcase-minimized-shape_fuzzer-6126814756995072.dxf"
        )
        lyr = ds.GetLayer(0)
        for f in lyr:
            pass


###############################################################################


def test_ogr_dxf_read_closed_polyline_with_bulge():
    """Test https://github.com/OSGeo/gdal/issues/10153"""

    ds = ogr.Open("data/dxf/closed_polyline_with_bulge.dxf")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetX(0) == g.GetX(g.GetPointCount() - 1)
    assert g.GetY(0) == g.GetY(g.GetPointCount() - 1)
    assert (
        g.ExportToWkt()
        == "LINESTRING (40585366.7065058 3433935.53809098,40585329.9256486 3433998.44081707,40585329.9256486 3433998.44081707,40585328.5387678 3434000.63680805,40585327.0051198 3434002.73293274,40585325.3318693 3434004.71939884,40585323.526833 3434006.58692634,40585321.5984435 3434008.32679087,40585319.5557093 3434009.93086443,40585317.4081735 3434011.39165342,40585315.1658683 3434012.70233358,40585312.8392691 3434013.85678191,40585310.4392448 3434014.84960528,40585307.9770074 3434015.67616559,40585305.4640596 3434016.33260146,40585302.9121409 3434016.81584629,40585300.3331728 3434017.12364253,40585297.7392033 3434017.25455227,40585271.1313178 3434017.68678191,40585252.1698149 3433885.99037548,40585256.74147 3433885.9161116,40585256.74147 3433885.9161116,40585266.2920614 3433886.0916242,40585275.8076317 3433886.92740148,40585285.2425893 3433888.41943902,40585294.551729 3433890.56058809,40585303.6904483 3433893.34058991,40585312.6149614 3433896.74612477,40585321.2825086 3433900.76087591,40585329.6515615 3433905.36560764,40585364.2483736 3433925.99220872,40585364.2483736 3433925.99220872,40585364.6481964 3433926.24937651,40585365.0296424 3433926.53308859,40585365.3909523 3433926.84203644,40585365.7304596 3433927.17479516,40585366.0465985 3433927.52983003,40585366.337911 3433927.90550359,40585366.6030535 3433928.30008319,40585366.840803 3433928.71174899,40585367.0500632 3433929.13860232,40585367.2298688 3433929.5786745,40585367.3793906 3433930.02993587,40585367.4979389 3433930.49030515,40585367.5849671 3433930.95765907,40585367.6400736 3433931.42984214,40585367.6630045 3433931.9046766,40585367.6536538 3433932.37997246,40585367.6120647 3433932.85353759,40585367.5384291 3433933.32318787,40585367.4330866 3433933.7867572,40585367.2965229 3433934.24210757,40585367.129368 3433934.68713883,40585366.9323928 3433935.11979846,40585366.7065058 3433935.53809098)"
    )

    ds = gdal.OpenEx(
        "data/dxf/closed_polyline_with_bulge.dxf",
        open_options=["CLOSED_LINE_AS_POLYGON=YES"],
    )
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == ogr.wkbPolygon


###############################################################################


def test_ogr_dxf_write_INSUNITS(tmp_vsimem):

    filename = str(tmp_vsimem / "out.dxf")

    with ogr.GetDriverByName("DXF").CreateDataSource(
        filename, options=["INSUNITS=METERS"]
    ) as ds:
        pass
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$INSUNITS", "DXF_HEADER_VARIABLES") == "6"

    with ogr.GetDriverByName("DXF").CreateDataSource(
        filename, options=["INSUNITS=21"]
    ) as ds:
        pass
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$INSUNITS", "DXF_HEADER_VARIABLES") == "21"

    with gdal.quiet_errors():
        with gdal.GetDriverByName("DXF").Create(
            filename, 0, 0, 0, gdal.GDT_Unknown, options=["INSUNITS=INVALID"]
        ) as ds:
            pass
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$INSUNITS", "DXF_HEADER_VARIABLES") == "     1"

    with ogr.GetDriverByName("DXF").CreateDataSource(filename) as ds:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(32631)
        ds.CreateLayer("test", srs=srs)
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$INSUNITS", "DXF_HEADER_VARIABLES") == "6"

    with ogr.GetDriverByName("DXF").CreateDataSource(filename) as ds:
        srs = osr.SpatialReference()
        srs.ImportFromProj4("+proj=merc +units=ft")
        ds.CreateLayer("test", srs=srs)
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$INSUNITS", "DXF_HEADER_VARIABLES") == "2"

    with ogr.GetDriverByName("DXF").CreateDataSource(filename) as ds:
        srs = osr.SpatialReference()
        srs.ImportFromProj4("+proj=merc +units=us-ft")
        ds.CreateLayer("test", srs=srs)
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$INSUNITS", "DXF_HEADER_VARIABLES") == "21"

    # No CRS
    with ogr.GetDriverByName("DXF").CreateDataSource(filename) as ds:
        pass
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$INSUNITS", "DXF_HEADER_VARIABLES") == "     1"

    # Not a projected CRS
    with ogr.GetDriverByName("DXF").CreateDataSource(filename) as ds:
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        ds.CreateLayer("test", srs=srs)
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$INSUNITS", "DXF_HEADER_VARIABLES") == "     1"

    # Unknown linear units
    with gdal.quiet_errors():
        with ogr.GetDriverByName("DXF").CreateDataSource(filename) as ds:
            srs = osr.SpatialReference()
            srs.ImportFromProj4("+proj=merc +to_meter=2")
            ds.CreateLayer("test", srs=srs)
        with ogr.Open(filename) as ds:
            assert ds.GetMetadataItem("$INSUNITS", "DXF_HEADER_VARIABLES") == "     1"


###############################################################################


def test_ogr_dxf_write_MEASUREMENT(tmp_vsimem):

    filename = str(tmp_vsimem / "out.dxf")

    with ogr.GetDriverByName("DXF").CreateDataSource(
        filename, options=["MEASUREMENT=METRIC"]
    ) as ds:
        pass
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$MEASUREMENT", "DXF_HEADER_VARIABLES") == "1"

    with ogr.GetDriverByName("DXF").CreateDataSource(
        filename, options=["MEASUREMENT=1"]
    ) as ds:
        pass
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$MEASUREMENT", "DXF_HEADER_VARIABLES") == "1"

    with gdal.quiet_errors():
        with ogr.GetDriverByName("DXF").CreateDataSource(
            filename, options=["MEASUREMENT=INVALID"]
        ) as ds:
            pass
    with ogr.Open(filename) as ds:
        assert ds.GetMetadataItem("$MEASUREMENT", "DXF_HEADER_VARIABLES") == "     0"


###############################################################################
# Use case of https://github.com/OSGeo/gdal/issues/11591
# Test reading a INSERT block whose column count is zero.
# Interpretating it as 1, as AutoCAD does


@gdaltest.enable_exceptions()
def test_ogr_dxf_insert_col_count_zero():

    with ogr.Open("data/dxf/insert_only_col_count_zero.dxf") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with gdal.config_option("DXF_INLINE_BLOCKS", "NO"):
        with ogr.Open("data/dxf/insert_only_col_count_zero.dxf") as ds:
            lyr = ds.GetLayerByName("blocks")
            assert lyr.GetFeatureCount() == 1


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_dxf_read_binary_dxf_r12():

    with ogr.Open("data/dxf/bin_dxf_r12.dxf") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 3


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_dxf_read_binary_dxf_r2000():

    with ogr.Open("data/dxf/bin_dxf_r2000.dxf") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_dxf_convert_from_binary_dxf_r12(tmp_vsimem):

    gdal.VectorTranslate(tmp_vsimem / "out.dxf", "data/dxf/bin_dxf_r12.dxf")
    with ogr.Open(tmp_vsimem / "out.dxf") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 3


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_dxf_convert_from_binary_dxf_r2000(tmp_vsimem):

    gdal.VectorTranslate(tmp_vsimem / "out.dxf", "data/dxf/bin_dxf_r2000.dxf")
    with ogr.Open(tmp_vsimem / "out.dxf") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_dxf_read_wipeout_binary():

    ds = ogr.Open("data/dxf/BINARY_wipeout.dxf")
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON ((448381.028869725 6913933.17804321,448381.232017696 6913933.39891582,448380.807997101 6913933.38119118,448381.028869725 6913933.17804321,448381.011145071 6913933.6020638,448381.232017696 6913933.39891582,448381.028869725 6913933.17804321))",
    )

    feat = lyr.GetNextFeature()
    ogrtest.check_feature_geometry(
        feat,
        "POLYGON ((448380.538954307 6913930.73282502,448380.538954307 6913930.73282502,448380.538954307 6913931.73282502,448381.538954307 6913931.73282502,448381.538954307 6913930.73282502,448380.538954307 6913930.73282502))",
    )


###############################################################################
# Test reading transparency 440 block


@gdaltest.enable_exceptions()
def test_ogr_dxf_read_transparency():

    with ogr.Open("data/dxf/transparency.dxf") as ds:
        lyr = ds.GetLayer(0)
        feat = lyr.GetNextFeature()
        assert feat.GetStyleString() == "PEN(c:#ffbeb87f)"


###############################################################################
# Test writing true color and transparency


@gdaltest.enable_exceptions()
def test_ogr_dxf_write_true_color_and_transparency(tmp_path):

    # Check that we perfectly roundtry transparency.dxf
    gdal.VectorTranslate(
        tmp_path / "out.dxf",
        "data/dxf/transparency.dxf",
        datasetCreationOptions=["FIRST_ENTITY=131072"],
    )

    with gdal.VSIFile(tmp_path / "out.dxf", "rb") as fout, gdal.VSIFile(
        "data/dxf/transparency.dxf", "rb"
    ) as fin:
        assert fout.read() == fin.read()


###############################################################################
# Test hatch pattern support


@gdaltest.enable_exceptions()
@pytest.mark.parametrize(
    "style_string",
    [
        'BRUSH(fc:#00000000,id:"ogr-brush-1")',
        'BRUSH(fc:#ff0000,bc:#0000ff,id:"ogr-brush-2")',  # using indexed colors
        'BRUSH(fc:#123456,bc:#7890ab,id:"ogr-brush-3")',
        'BRUSH(fc:#123456,bc:#7890ab,id:"ogr-brush-4")',
        'BRUSH(fc:#123456,bc:#7890ab,id:"ogr-brush-5")',
        'BRUSH(fc:#123456,bc:#7890ab,id:"ogr-brush-6")',
        'BRUSH(fc:#123456,bc:#7890ab,id:"ogr-brush-7")',
        'BRUSH(fc:#123456,bc:#7890ab,id:"ogr-brush-7",a:10.000000)',
        'BRUSH(fc:#123456,bc:#7890ab,id:"ogr-brush-7",s:5.000000)',
    ],
)
def test_ogr_dxf_hatch_pattern(tmp_path, style_string):

    with ogr.GetDriverByName("DXF").CreateDataSource(tmp_path / "hatch.dxf") as ds:
        lyr = ds.CreateLayer("test")
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 10,10 10,10 0,0 0))"))
        f.SetStyleString(style_string)
        lyr.CreateFeature(f)

    with ogr.Open(tmp_path / "hatch.dxf") as ds:
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToWkt() == "POLYGON ((0 0,0 10,10 10,10 0,0 0))"
        assert f.GetStyleString() == style_string


###############################################################################
# Test hatch pattern support


@gdaltest.enable_exceptions()
def test_ogr_dxf_hatch_pattern_read():
    with ogr.Open("data/dxf/hatch_pattern_generated_by_gdal.dxf") as ds:
        lyr = ds.GetLayer(0)
        styles = [f.GetStyleString() for f in lyr]
        expected_styles = [
            "BRUSH(fc:#ff00ff)",
            'BRUSH(fc:#ff0000,bc:#7f7f7f,id:"ogr-brush-7")',
            'BRUSH(fc:#ff0000,bc:#0000ff,id:"ogr-brush-6")',
            'BRUSH(fc:#00ff00,bc:#123456,id:"ogr-brush-5")',
            'BRUSH(fc:#ff0000,bc:#00ff00,id:"ogr-brush-4")',
            'BRUSH(fc:#ffff00,bc:#123456,id:"ogr-brush-3")',
            'BRUSH(fc:#ff0000,bc:#0080ff,id:"ogr-brush-2",s:0.500000)',
        ]
        assert styles == expected_styles
