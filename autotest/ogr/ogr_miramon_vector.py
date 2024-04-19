#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR MiraMon vector driver.
# Author:   Abel Pau <a dot pau at creaf.uab.cat>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
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

# import os
# import pdb

import gdaltest

# import ogrtest
import pytest

# from osgeo import gdal, ogr, osr
from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("MiraMonVector")

###############################################################################
# basic point test


def check_simple_point(ds):

    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 3
    assert lyr.GetGeomType() == ogr.wkbPoint

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 0
    assert (
        f.GetGeometryRef().ExportToWkt() == "POINT (513.488106565226 848.806850618409)"
    )
    assert f.GetField("ID_GRAFIC") == 0
    assert f.GetFieldAsString("ATT1") == "A"
    assert f.GetFieldAsString("ATTRIBUTE_2") == "B"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert (
        f.GetGeometryRef().ExportToWkt() == "POINT (342.325404376834 715.680304471881)"
    )
    assert f.GetField("ID_GRAFIC") == 1
    assert f.GetFieldAsString("ATT1") == "C"
    assert f.GetFieldAsString("ATTRIBUTE_2") == "D"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert (
        f.GetGeometryRef().ExportToWkt() == "POINT (594.503182156354 722.692543360232)"
    )
    assert f.GetField("ID_GRAFIC") == 2
    assert f.GetFieldAsString("ATT1") == ""
    assert f.GetFieldAsString("ATTRIBUTE_2") == ""


def test_ogr_miramon_read_simple_point():

    ds = gdal.OpenEx("data/miramon/Points/SimplePoints/SimplePointsFile.pnt")
    assert ds is not None, "Failed to get dataset"

    check_simple_point(ds)


def test_ogr_miramon_write_simple_pointV11(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pnt")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Points/SimplePoints/SimplePointsFile.pnt",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_point(ds)


def test_ogr_miramon_write_simple_pointV20(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pnt")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Points/SimplePoints/SimplePointsFile.pnt",
        format="MiraMonVector",
        options="-lco Version=V2.0",
    )

    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_point(ds)


###############################################################################
# basic linestring test


def check_simple_arc(ds):

    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 4
    assert lyr.GetGeomType() == ogr.wkbLineString

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 0
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "LINESTRING (351.333967649907 610.58039961936,474.450999048575 824.784015223546,758.721217887776 838.797335870549,1042.99143672698 610.58039961936,1369.30161750719 562.534728829636)"
    )
    assert f.GetField("ID_GRAFIC") == 0
    assert f.GetField("N_VERTEXS") == 5
    assert f.GetField("LONG_ARC") == pytest.approx(1226.052754666, abs=1e-5)
    assert f.GetField("NODE_INI") == 0
    assert f.GetField("NODE_FI") == 1
    assert f.GetFieldAsString("ATT1") == "A"
    assert f.GetFieldAsString("ATT2") == "B"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 1
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "LINESTRING (794.755470980069 442.420551855326,613.583254043818 399.379638439531,642.61084681261 212.201712654565,861.819219790726 201.191246431919,1041.99048525219 460.437678401472,598.568981922029 591.562321598428,1109.05423406285 931.88582302564)"
    )
    assert f.GetField("ID_GRAFIC") == 1
    assert f.GetField("N_VERTEXS") == 7
    assert f.GetField("LONG_ARC") == pytest.approx(1986.750568, abs=1e-5)
    assert f.GetField("NODE_INI") == 2
    assert f.GetField("NODE_FI") == 3
    assert f.GetFieldAsString("ATT1") == "C"
    assert f.GetFieldAsString("ATT2") == "D"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 2
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "LINESTRING (887.843958135159 858.816365366268,989.941008563323 767.729781160749)"
    )
    assert f.GetField("ID_GRAFIC") == 2
    assert f.GetField("N_VERTEXS") == 2
    assert f.GetField("LONG_ARC") == pytest.approx(136.823147, abs=1e-5)
    assert f.GetField("NODE_INI") == 4
    assert f.GetField("NODE_FI") == 5
    assert f.GetFieldAsString("ATT1") == "C"
    assert f.GetFieldAsString("ATT2") == "D"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 3
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "LINESTRING (537.510941960088 719.684110371025,496.471931493865 633.602283539436,432.411037107567 572.544243577495,415.394862036206 631.600380589864,492.468125594722 642.610846812509,564.536631779308 630.599429115078)"
    )
    assert f.GetField("ID_GRAFIC") == 3
    assert f.GetField("N_VERTEXS") == 6
    assert f.GetField("LONG_ARC") == pytest.approx(396.238966, abs=1e-5)
    assert f.GetField("NODE_INI") == 6
    assert f.GetField("NODE_FI") == 7
    assert f.GetFieldAsString("ATT1") == "E"
    assert f.GetFieldAsString("ATT2") == "F"


def test_ogr_miramon_read_simple_arc():

    ds = gdal.OpenEx("data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc")
    assert ds is not None, "Failed to get dataset"
    check_simple_arc(ds)


def test_ogr_miramon_write_simple_arcV11(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.arc")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_arc(ds)
    del ds


def test_ogr_miramon_write_simple_arcV20(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.arc")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc",
        format="MiraMonVector",
        options="-lco Version=V2.0",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_arc(ds)
    del ds


###############################################################################
# basic polygon test


def check_simple_polygon(ds):

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 3
    assert lyr.GetGeomType() == ogr.wkbPolygon

    # going to the first polygon
    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 0
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((335.318744053333 769.731684110321,552.525214081877 856.814462416696,775.737392959137 707.672692673594,648.616555661325 493.469077069408,386.367269267414 498.473834443337,335.318744053333 769.731684110321))"
    )
    assert f.GetField("ID_GRAFIC") == 1
    assert f.GetField("N_VERTEXS") == 6
    assert f.GetField("PERIMETRE") == pytest.approx(1289.866489495, abs=1e-5)
    assert f.GetField("AREA") == pytest.approx(112471.221989, abs=1e-5)
    assert f.GetField("N_ARCS") == 1
    assert f.GetField("N_POLIG") == 1
    assert f.GetFieldAsString("ATT1") == "A"
    assert f.GetFieldAsString("ATT2") == "B"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 1
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((1068.01522359662 849.807802093194,1160.10275927693 795.756422454755,1224.16365366323 682.648905803946,1156.09895337779 525.499524262557,962.915318744103 489.465271170264,830.789724072362 617.587059942862,924.879162702239 740.704091341529,1068.01522359662 849.807802093194))"
    )
    assert f.GetField("ID_GRAFIC") == 2
    assert f.GetField("N_VERTEXS") == 8
    assert f.GetField("PERIMETRE") == pytest.approx(1123.514024, abs=1e-5)
    assert f.GetField("AREA") == pytest.approx(88563.792204, abs=1e-5)
    assert f.GetField("N_ARCS") == 1
    assert f.GetField("N_POLIG") == 1
    assert f.GetFieldAsString("ATT1") == "C"
    assert f.GetFieldAsString("ATT2") == "D"

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 2
    assert (
        f.GetGeometryRef().ExportToWkt()
        == "POLYGON ((636.605137963894 390.371075166458,580.551855375883 575.547098001853,723.687916270269 594.565176022785,796.757373929641 475.451950523261,744.707897240773 396.376784015173,636.605137963894 390.371075166458))"
    )
    assert f.GetField("ID_GRAFIC") == 3
    assert f.GetField("N_VERTEXS") == 6
    assert f.GetField("PERIMETRE") == pytest.approx(680.544697, abs=1e-5)
    assert f.GetField("AREA") == pytest.approx(30550.052343, abs=1e-5)
    assert f.GetField("N_ARCS") == 1
    assert f.GetField("N_POLIG") == 1
    assert f.GetFieldAsString("ATT1") == "C"
    assert f.GetFieldAsString("ATT2") == "D"


def test_ogr_miramon_read_simple_polygon():

    ds = gdal.OpenEx(
        "data/miramon/Polygons/SimplePolygons/SimplePolFile.pol", gdal.OF_VECTOR
    )
    assert ds is not None, "Failed to get dataset"
    check_simple_polygon(ds)


def test_ogr_miramon_write_simple_polygonV11(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pol")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Polygons/SimplePolygons/SimplePolFile.pol",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_polygon(ds)


def test_ogr_miramon_write_simple_polygonV20(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pol")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Polygons/SimplePolygons/SimplePolFile.pol",
        format="MiraMonVector",
        options="-lco Version=V2.0",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_simple_polygon(ds)


###############################################################################
# testing empty layers


def test_ogr_miramon_empty_point_layers():

    ds = gdal.OpenEx("data/miramon/Points/EmptyPoints/Empty_PNT.pnt", gdal.OF_VECTOR)
    assert ds is not None, "Failed to get dataset"

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 0

    f = lyr.GetNextFeature()
    assert f is None, "Failed to get empty feature"

    ds = None


def test_ogr_miramon_empty_arc_layers():

    ds = gdal.OpenEx("data/miramon/Arcs/EmptyArcs/Empty_ARC.arc", gdal.OF_VECTOR)
    assert ds is not None, "Failed to get dataset"

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 0

    f = lyr.GetNextFeature()
    assert f is None, "Failed to get empty feature"

    ds = None


def test_ogr_miramon_empty_pol_layers():

    ds = gdal.OpenEx(
        "data/miramon/Polygons/EmptyPolygons/Empty_POL.pol", gdal.OF_VECTOR
    )
    assert ds is not None, "Failed to get dataset"

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    # The layer has no features
    assert lyr.GetFeatureCount() == 0

    f = lyr.GetNextFeature()
    assert f is None, "Failed to get empty feature"

    ds = None


###############################################################################
# testing 3d part


def check_3d_point(ds):

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 31
    assert lyr.GetGeomType() == ogr.wkbPoint25D

    f = lyr.GetNextFeature()
    assert f is not None, "Failed to get feature"
    assert f.GetFID() == 0

    assert f.GetGeometryRef().ExportToWkt() == "POINT (440551.66 4635315.3 619.96)"

    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetCoordinateDimension() == 3
    assert g.GetZ() == 619.96

    f = lyr.GetFeature(30)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetZ() == 619.77


def test_ogr_miramon_read_3d_point(tmp_vsimem):

    ds = gdal.OpenEx("data/miramon/Points/3dpoints/Some3dPoints.pnt", gdal.OF_VECTOR)
    assert ds is not None, "Failed to get dataset"
    check_3d_point(ds)


def test_ogr_miramon_write_3d_point(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pnt")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Points/3dpoints/Some3dPoints.pnt",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_3d_point(ds)


def check_3d_arc(ds):

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 6
    assert lyr.GetGeomType() == ogr.wkbLineString25D

    f = lyr.GetFeature(0)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetCoordinateDimension() == 3
    assert g.GetPointCount() == 4
    p = g.GetPoint(0)
    assert p[2] == 595.1063842773438
    p = g.GetPoint(1)
    assert p[2] == 326.656005859375
    p = g.GetPoint(2)
    assert p[2] == 389.99432373046875
    p = g.GetPoint(3)
    assert p[2] == 716.6224975585938

    f = lyr.GetFeature(5)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetCoordinateDimension() == 3
    assert g.GetPointCount() == 2
    p = g.GetPoint(0)
    assert p[2] == 233.82064819335938
    p = g.GetPoint(1)
    assert p[2] == 794.5372314453125

    ds = None


def test_ogr_miramon_read_3d_arc(tmp_vsimem):

    ds = gdal.OpenEx("data/miramon/Arcs/3dArcs/linies_3d_WGS84.arc", gdal.OF_VECTOR)
    assert ds is not None, "Failed to get dataset"
    check_3d_arc(ds)


def test_ogr_miramon_write_3d_arc(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.arc")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Arcs/3dArcs/linies_3d_WGS84.arc",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_3d_arc(ds)
    del ds


def check_3d_pol(ds):

    lyr = ds.GetLayer(0)

    assert lyr is not None, "Failed to get layer"

    assert lyr.GetFeatureCount() == 5
    assert lyr.GetGeomType() == ogr.wkbPolygon25D

    f = lyr.GetFeature(0)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetCoordinateDimension() == 3
    r = g.GetGeometryRef(0)
    assert r is not None, "Failed to get geometry"
    assert r.GetPointCount() == 4
    p = r.GetPoint(0)
    assert p[2] == 11.223576545715332
    p = r.GetPoint(1)
    assert p[2] == 9.221868515014648
    p = r.GetPoint(2)
    assert p[2] == 21.929399490356445
    p = r.GetPoint(3)
    assert p[2] == 11.223576545715332

    f = lyr.GetFeature(4)
    assert f is not None, "Failed to get feature"
    g = f.GetGeometryRef()
    assert g is not None, "Failed to get geometry"
    assert g.GetCoordinateDimension() == 3
    r = g.GetGeometryRef(0)
    assert r is not None, "Failed to get geometry"
    assert r.GetPointCount() == 4
    p = r.GetPoint(0)
    assert p[2] == 18.207277297973633
    p = r.GetPoint(1)
    assert p[2] == 21.929399490356445
    p = r.GetPoint(2)
    assert p[2] == 5.746463775634766
    p = r.GetPoint(3)
    assert p[2] == 18.207277297973633


def test_ogr_miramon_read_3d_pol():

    ds = gdal.OpenEx("data/miramon/Polygons/3dPolygons/tin_3d.pol", gdal.OF_VECTOR)
    assert ds is not None, "Failed to get dataset"
    check_3d_pol(ds)


def test_ogr_miramon_write_3d_pol(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.pol")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Polygons/3dPolygons/tin_3d.pol",
        format="MiraMonVector",
    )
    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    check_3d_pol(ds)
    del ds


###############################################################################
# ogrsf test in some files


@pytest.mark.parametrize(
    "filename",
    [
        "Points/3dpoints/Some3dPoints.pnt",
        "Points/SimplePoints/SimplePointsFile.pnt",
        "Points/EmptyPoints/Empty_PNT.pnt",
        "Arcs/SimpleArcs/SimpleArcFile.arc",
        "Arcs/EmptyArcs/Empty_ARC.arc",
        "Arcs/3dArcs/linies_3d_WGS84.arc",
        "Polygons/SimplePolygons/SimplePolFile.pol",
        "Polygons/EmptyPolygons/Empty_POL.pol",
        "Polygons/3dPolygons/tin_3d.pol",
    ],
)
def test_ogr_miramon_test_ogrsf(filename):

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip("test_ogrsf not available")

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/miramon/" + filename
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# -lco tests: CreationLanguage


@pytest.mark.parametrize(
    "Language, expected_description",
    [
        ("CAT", "Identificador Gràfic intern"),
        ("SPA", "Identificador Gráfico interno"),
        ("ENG", "Internal Graphic identifier"),
    ],
)
def test_ogr_miramon_CreationLanguage(tmp_vsimem, Language, expected_description):

    out_filename = str(tmp_vsimem / "out.pnt")
    gdal.VectorTranslate(
        out_filename,
        "data/miramon/Points/SimplePoints/SimplePointsFile.pnt",
        format="MiraMonVector",
        options="-lco CreationLanguage=" + Language,
    )

    ds = gdal.OpenEx(out_filename, gdal.OF_VECTOR)
    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    layer_def = lyr.GetLayerDefn()
    field_index = layer_def.GetFieldIndex("ID_GRAFIC")
    assert field_index >= 0

    field_def = layer_def.GetFieldDefn(field_index)
    field_description = field_def.GetAlternativeNameRef()
    assert field_description == expected_description


###############################################################################
# -lco tests: CreationLanguage


@pytest.mark.parametrize(
    "Language,expected_description",
    [
        ("CAT", "Identificador Gràfic intern"),
        ("SPA", "Identificador Gráfico interno"),
        ("ENG", "Internal Graphic identifier"),
    ],
)
def test_ogr_miramon_OpenLanguagePoint(Language, expected_description):

    ds = gdal.OpenEx(
        "data/miramon/Points/SimplePoints/SimplePointsFile.pnt",
        gdal.OF_VECTOR,
        open_options=["OpenLanguage=" + Language],
    )
    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    layer_def = lyr.GetLayerDefn()
    field_index = layer_def.GetFieldIndex("ID_GRAFIC")
    assert field_index >= 0

    field_def = layer_def.GetFieldDefn(field_index)
    field_description = field_def.GetAlternativeNameRef()
    assert field_description == expected_description


@pytest.mark.parametrize(
    "Language,expected_description",
    [
        ("CAT", "Node inicial"),
        ("SPA", "Nodo inicial"),
        ("ENG", "Initial node"),
    ],
)
def test_ogr_miramon_OpenLanguageArc(Language, expected_description):

    ds = gdal.OpenEx(
        "data/miramon/Arcs/SimpleArcs/SimpleArcFile.arc",
        gdal.OF_VECTOR,
        open_options=["OpenLanguage=" + Language],
    )
    lyr = ds.GetLayer(0)
    assert lyr is not None, "Failed to get layer"

    layer_def = lyr.GetLayerDefn()
    field_index = layer_def.GetFieldIndex("NODE_INI")
    assert field_index >= 0

    field_def = layer_def.GetFieldDefn(field_index)
    field_description = field_def.GetAlternativeNameRef()
    assert field_description == expected_description
