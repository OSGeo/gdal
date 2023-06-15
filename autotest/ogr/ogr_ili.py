#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Interlis driver testing.
# Author:   Pirmin Kalberer <pka(at)sourcepole.ch>
#
###############################################################################
# Copyright (c) 2012, Pirmin Kalberer <pka(at)sourcepole.ch>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################


import gdaltest
import ogrtest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("Interlis 1")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():
    yield

    gdaltest.clean_tmp()


###############################################################################
# Check that Ili1 point layer is properly read.


@pytest.mark.require_geos
def test_ogr_interlis1_2():

    ds = ogr.Open("data/ili/Beispiel.itf,data/ili/Beispiel.imd")
    layers = [
        "Bodenbedeckung__BoFlaechen",
        "Bodenbedeckung__BoFlaechen_Form",
        "Bodenbedeckung__Strasse",
        "Bodenbedeckung__Gebaeude",
    ]
    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

    lyr = ds.GetLayerByName("Bodenbedeckung__BoFlaechen")

    assert lyr.GetFeatureCount() == 3, "feature count wrong."

    # Get 2nd feature
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()

    field_values = [20, 1, 168.27, 170.85]

    assert feat.GetFieldCount() == len(field_values), "field count wrong."

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
            feat.DumpReadable()
            print(feat.GetFieldAsString(i))
            pytest.fail("field value wrong.")

    geom = feat.GetGeomFieldRef(0)
    assert geom.GetCoordinateDimension() == 2, "dimension wrong."
    geom = feat.GetGeomFieldRef(1)
    assert geom.GetCoordinateDimension() == 2, "dimension wrong."

    geom = feat.GetGeometryRef()
    assert geom.GetGeometryName() == "POLYGON", "Geometry of wrong type."


###############################################################################
# Ili1 FORMAT DEFAULT test.


def test_ogr_interlis1_3():

    ds = ogr.Open("data/ili/format-default.itf,data/ili/format-default.imd")

    layers = ["FormatTests__FormatTable"]
    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

    lyr = ds.GetLayerByName("FormatTests__FormatTable")

    assert lyr.GetFeatureCount() == 1, "feature count wrong."

    feat = lyr.GetNextFeature()

    field_values = [0, "aa bb", "cc^dd", "", 1]

    assert feat.GetFieldCount() == len(field_values), "field count wrong."

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
            feat.DumpReadable()
            print(feat.GetFieldAsString(i))
            pytest.fail("field value wrong.")


###############################################################################
# Ili1 FORMAT test.


def test_ogr_interlis1_4():

    ds = ogr.Open("data/ili/format-test.itf,data/ili/format-test.imd")

    layers = ["FormatTests__FormatTable"]
    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

    lyr = ds.GetLayerByName("FormatTests__FormatTable")

    assert lyr.GetFeatureCount() == 1, "feature count wrong."

    feat = lyr.GetNextFeature()

    field_values = [0, "aa_bb", "cc dd", "", 1]

    assert feat.GetFieldCount() == len(field_values), "field count wrong."

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
            feat.DumpReadable()
            print(feat.GetFieldAsString(i))
            pytest.fail("field value wrong.")


###############################################################################
# Write Ili1 transfer file without model.


def test_ogr_interlis1_5():

    ds = ogr.Open("data/ili/format-default.itf,data/ili/format-default.imd")

    lyr = ds.GetLayerByName("FormatTests__FormatTable")
    feat = lyr.GetNextFeature()

    driver = ogr.GetDriverByName("Interlis 1")
    outfile = "tmp/interlis1_5.itf"

    with gdaltest.error_handler():
        dst_ds = driver.CreateDataSource(outfile)

    dst_lyr = dst_ds.CreateLayer("FormatTests__FormatTable")

    layer_defn = lyr.GetLayerDefn()
    for i in range(layer_defn.GetFieldCount()):
        dst_lyr.CreateField(layer_defn.GetFieldDefn(i))
    dst_feat = ogr.Feature(feature_def=dst_lyr.GetLayerDefn())
    dst_feat.SetFrom(feat)
    dst_lyr.CreateFeature(dst_feat)

    dst_ds = None

    with open(outfile) as f:
        itf = f.read()
        expected = """MTID INTERLIS1
MODL OGR
ETOP
TOPI FormatTests
TABL FormatTable
OBJE 0 0 aa_bb cc^dd @ 1
ETAB
ETOP
EMOD
ENDE"""
        assert expected in itf, "Interlis output doesn't match."


###############################################################################
# Write Ili1 transfer file.


def test_ogr_interlis1_6():

    ds = ogr.Open("data/ili/format-default.itf,data/ili/format-default.imd")
    lyr = ds.GetLayerByName("FormatTests__FormatTable")
    feat = lyr.GetNextFeature()

    driver = ogr.GetDriverByName("Interlis 1")
    outfile = "tmp/interlis1_6.itf"
    dst_ds = driver.CreateDataSource(outfile + ",data/ili/format-default.imd")

    dst_lyr = dst_ds.CreateLayer("test")

    layer_defn = lyr.GetLayerDefn()
    for i in range(layer_defn.GetFieldCount()):
        dst_lyr.CreateField(layer_defn.GetFieldDefn(i))
    dst_feat = ogr.Feature(feature_def=dst_lyr.GetLayerDefn())
    dst_feat.SetFrom(feat)
    dst_lyr.CreateFeature(dst_feat)

    dst_ds = None

    with open(outfile) as f:
        itf = f.read()
        expected = """MTID INTERLIS1
MODL FormatDefault
TOPI FormatTests
TABL test
OBJE 1 0 aa_bb cc^dd @ 1
ETAB
ETOP
EMOD
ENDE"""
        assert expected in itf, "Interlis output doesn't match."


###############################################################################
# Ili1 character encoding test.


def test_ogr_interlis1_7():

    ds = ogr.Open("data/ili/encoding-test.itf,data/ili/format-default.imd")

    layers = ["FormatTests__FormatTable"]
    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

    lyr = ds.GetLayerByName("FormatTests__FormatTable")

    assert lyr.GetFeatureCount() == 1, "feature count wrong."

    feat = lyr.GetNextFeature()

    # Interlis 1 Encoding is ISO 8859-1 (Latin1)
    # Python source code is UTF-8 encoded
    field_values = [0, "äöü", "ÄÖÜ", "", 1]

    assert feat.GetFieldCount() == len(field_values), "field count wrong."

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
            feat.DumpReadable()
            print(feat.GetFieldAsString(i))
            pytest.fail("field value wrong.")

    # Write back
    driver = ogr.GetDriverByName("Interlis 1")
    outfile = "tmp/interlis1_7.itf"
    dst_ds = driver.CreateDataSource(outfile + ",data/ili/format-default.imd")

    dst_lyr = dst_ds.CreateLayer("FormatTests__FormatTable")

    layer_defn = lyr.GetLayerDefn()
    for i in range(layer_defn.GetFieldCount()):
        dst_lyr.CreateField(layer_defn.GetFieldDefn(i))
    dst_feat = ogr.Feature(feature_def=dst_lyr.GetLayerDefn())
    dst_feat.SetFrom(feat)
    dst_lyr.CreateFeature(dst_feat)

    dst_ds = None

    try:
        # Python 3
        f = open(outfile, encoding="iso-8859-1")
    except TypeError:
        f = open(outfile)
    itf = f.read()
    expected = """MTID INTERLIS1
MODL FormatDefault
TABL FormatTable
OBJE 2 0 äöü ÄÖÜ @ 1
ETAB
ETOP
EMOD
ENDE"""
    try:
        # Python 2
        expected = expected.decode("utf8").encode("iso-8859-1")
    except Exception:
        pass
    assert expected in itf, "Interlis output doesn't match."


###############################################################################
# Ili1 VRT rename


def test_ogr_interlis1_9():

    ds = ogr.Open("data/ili/Beispiel-rename.vrt")
    layers = ["BoGebaeude"]
    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

    lyr = ds.GetLayerByName("BoGebaeude")

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef() == "AssekuranzNr", (
        "Wrong field name: " + lyr.GetLayerDefn().GetFieldDefn(0).GetNameRef()
    )

    assert lyr.GetFeatureCount() == 1, "feature count wrong."

    feat = lyr.GetNextFeature()

    field_values = ["958", 10, "POINT (148.41 175.96)"]

    assert feat.GetFieldCount() == len(field_values) - 1, "field count wrong."

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
            feat.DumpReadable()
            print(feat.GetFieldAsString(i))
            pytest.fail("field value wrong.")


###############################################################################
# Ili1 Area with polygonizing


@pytest.mark.require_geos
def test_ogr_interlis1_10():

    ds = ogr.Open("data/ili/Beispiel.itf,data/ili/Beispiel.imd")

    lyr = ds.GetLayerByName("Bodenbedeckung__BoFlaechen")

    assert lyr.GetFeatureCount() == 3, "feature count wrong."

    feat = lyr.GetNextFeature()
    geom_field_values = [
        "POLYGON ((146.92 174.98,138.68 187.51,147.04 193.0,149.79 188.82,158.15 194.31,163.64 185.96,146.92 174.98))",
        "POINT (148.2 183.48)",
    ]

    assert feat.GetGeomFieldCount() == len(geom_field_values), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])


###############################################################################
# Ili1 multi-geom test (RFC41)


def test_ogr_interlis1_11():

    ds = ogr.Open("data/ili/multigeom.itf,data/ili/multigeom.imd")

    layers = ["MultigeomTests__MultigeomTable"]
    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

    lyr = ds.GetLayerByName("MultigeomTests__MultigeomTable")

    assert lyr.GetFeatureCount() == 1, "feature count wrong."

    feat = lyr.GetNextFeature()

    # feat.DumpReadable()
    #        _TID (String) = 0
    #        Text1 (String) = aa bb
    #        Number (Real) = 40
    #        MULTILINESTRING ((190.26 208.0 0, ...
    #        GeomPoint_0 (Real) = 148.41
    #        GeomPoint_1 (Real) = 175.96

    assert feat.GetFieldCount() == 5, "field count wrong."

    geom_columns = ["GeomLine", "GeomPoint"]

    assert feat.GetGeomFieldCount() == len(geom_columns), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        defn = lyr.GetLayerDefn().GetGeomFieldDefn(i)
        assert defn.GetName() == str(geom_columns[i]), "Geom field: " + defn.GetName()


###############################################################################
# Ili1 multi-geom test (RFC41)


def test_ogr_interlis1_12():

    ds = ogr.Open("data/ili/multicoord.itf,data/ili/multicoord.imd")

    layers = ["MulticoordTests__MulticoordTable"]
    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

    lyr = ds.GetLayerByName("MulticoordTests__MulticoordTable")

    assert lyr.GetFeatureCount() == 1, "feature count wrong."

    feat = lyr.GetNextFeature()

    if feat.GetFieldCount() != 6:
        feat.DumpReadable()
        pytest.fail("field count wrong.")

    geom_columns = ["coordPoint1", "coordPoint2"]

    assert feat.GetGeomFieldCount() == len(geom_columns), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        defn = lyr.GetLayerDefn().GetGeomFieldDefn(i)
        assert defn.GetName() == str(geom_columns[i]), "Geom field: " + defn.GetName()


###############################################################################
# Ili1 Surface test.


def test_ogr_interlis1_13():

    ds = ogr.Open("data/ili/surface.itf,data/ili/surface.imd")

    layers = [
        "SURFC_TOP__SURFC_TBL",
        "SURFC_TOP__SURFC_TBL_SHAPE",
        "SURFC_TOP__SURFC_TBL_TEXT_ID",
        "SURFC_TOP__SURFC_TBL_TEXT_ID_SHAPE",
        "SURFC_TOP__LineAttrib1",
        "SURFC_TOP__Flaechenelement",
        "SURFC_TOP__Flaechenelement_Geometrie",
    ]

    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

    lyr = ds.GetLayerByName("SURFC_TOP__SURFC_TBL_SHAPE")

    assert lyr.GetFeatureCount() == 5, "feature count wrong."

    lyr = ds.GetLayerByName("SURFC_TOP__SURFC_TBL")

    assert lyr.GetFeatureCount() == 4, "feature count wrong."

    feat = lyr.GetNextFeature()

    field_values = ["103", 1, 3, 1, 23, 25000, 20060111]

    assert feat.GetFieldCount() == len(field_values), "field count wrong."

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
            feat.DumpReadable()
            print(feat.GetFieldAsString(i))
            pytest.fail("field value wrong.")

    geom_field_values = [
        "CURVEPOLYGON (COMPOUNDCURVE ((598600.961 249487.174,598608.899 249538.768,598624.774 249594.331,598648.586 249630.05,598684.305 249661.8,598763.68 249685.612,598850.993 249685.612,598854.962 249618.143,598843.055 249550.675,598819.243 249514.956,598763.68 249479.237,598692.243 249447.487,598612.868 249427.643,598600.961 249487.174)))"
    ]

    assert feat.GetGeomFieldCount() == len(geom_field_values), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])

    # --- test curved polygon

    feat = lyr.GetNextFeature()
    geom_field_values = [
        "CURVEPOLYGON (COMPOUNDCURVE ((598131.445 249100.621,598170.131 249095.094,598200.448 249085.393),CIRCULARSTRING (598200.448 249085.393,598239.253 249062.352,598246.529 249044.162),(598246.529 249044.162,598245.316 249025.972,598229.552 249017.483,598165.28 249035.673,598134.963 249049.013,598130.273 249052.095,598131.445 249100.621)))"
    ]

    assert feat.GetGeomFieldCount() == len(geom_field_values), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])

    # --- test multi-ring polygon

    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    field_values = ["106", 3, 3, 1, 23, 25000, 20060111]

    assert feat.GetFieldCount() == len(field_values), "field count wrong."

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
            feat.DumpReadable()
            print(feat.GetFieldAsString(i))
            pytest.fail("field value wrong.")

    # FIXME: note this is an invalid curve polygon. The two rings are completely disjoint, so this should be a multisurface instead.
    geom_field_values = [
        "CURVEPOLYGON (COMPOUNDCURVE ((747951.24 265833.326,747955.101 265828.716,747954.975 265827.862,747951.166 265828.348,747951.24 265833.326)),COMPOUNDCURVE ((747925.762 265857.606,747927.618 265861.533,747928.237 265860.794,747930.956 265857.547,747925.762 265857.606)))"
    ]

    assert feat.GetGeomFieldCount() == len(geom_field_values), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])

    # --- same with text IDENT field
    # TODO: Surface with text IDENT field not supported yet

    # lyr = ds.GetLayerByName('SURFC_TOP__SURFC_TBL_TEXT_ID_SHAPE')

    # if lyr.GetFeatureCount() != 5:
    #     gdaltest.post_reason('feature count wrong.')
    #     return 'fail'

    # lyr = ds.GetLayerByName('SURFC_TOP__SURFC_TBL_TEXT_ID')

    # if lyr.GetFeatureCount() != 4:
    #     gdaltest.post_reason('feature count wrong.')
    #     return 'fail'

    # feat = lyr.GetNextFeature()

    # # Note: original value 'AAA_EZ20156' includes blank-symbol
    # field_values = ['AAA EZ20156', 1, 3, 1, 23, 25000, 20060111]

    # if feat.GetFieldCount() != len(field_values):
    #     gdaltest.post_reason( 'field count wrong.' )
    #     return 'fail'

    # for i in range(feat.GetFieldCount()):
    #     if feat.GetFieldAsString(i) != str(field_values[i]):
    #         feat.DumpReadable()
    #         print(feat.GetFieldAsString(i))
    #         gdaltest.post_reason( 'field value wrong.' )
    #         return 'fail'

    # geom_field_values = ['POLYGON ((598600.961 249487.174,598608.899 249538.768,598624.774 249594.331,598648.586 249630.05,598684.305 249661.8,598763.68 249685.612,598850.993 249685.612,598854.962 249618.143,598843.055 249550.675,598819.243 249514.956,598763.68 249479.237,598692.243 249447.487,598612.868 249427.643,598600.961 249487.174))']

    # if feat.GetGeomFieldCount() != len(geom_field_values):
    #     gdaltest.post_reason( 'geom field count wrong.' )
    #     print(feat.GetGeomFieldCount())
    #     return 'fail'

    # for i in range(feat.GetGeomFieldCount()):
    #     geom = feat.GetGeomFieldRef(i)
    #     if ogrtest.check_feature_geometry(geom, geom_field_values[i]) != 0:
    #         feat.DumpReadable()
    #         return 'fail'

    # # --- test multi-ring polygon

    # feat = lyr.GetNextFeature()
    # feat = lyr.GetNextFeature()
    # feat = lyr.GetNextFeature()
    # field_values = ['AAA EZ36360', 3, 3, 1, 23, 25000, 20060111]

    # if feat.GetFieldCount() != len(field_values):
    #     gdaltest.post_reason( 'field count wrong.' )
    #     return 'fail'

    # for i in range(feat.GetFieldCount()):
    #     if feat.GetFieldAsString(i) != str(field_values[i]):
    #         feat.DumpReadable()
    #         print(feat.GetFieldAsString(i))
    #         gdaltest.post_reason( 'field value wrong.' )
    #         return 'fail'

    # geom_field_values = ['POLYGON ((747925.762 265857.606,747927.618 265861.533,747928.237 265860.794,747930.956 265857.547,747925.762 265857.606),(747951.24 265833.326,747955.101 265828.716,747954.975 265827.862,747951.166 265828.348,747951.24 265833.326))']

    # if feat.GetGeomFieldCount() != len(geom_field_values):
    #     gdaltest.post_reason( 'geom field count wrong.' )
    #     print(feat.GetGeomFieldCount())
    #     return 'fail'

    # for i in range(feat.GetGeomFieldCount()):
    #     geom = feat.GetGeomFieldRef(i)
    #     if ogrtest.check_feature_geometry(geom, geom_field_values[i]) != 0:
    #         feat.DumpReadable()
    #         return 'fail'

    lyr = ds.GetLayerByName("SURFC_TOP__Flaechenelement_Geometrie")
    assert lyr.GetFeatureCount() == 3, "feature count wrong."

    feat = lyr.GetNextFeature()

    geom_field_values = [
        "MULTICURVE (COMPOUNDCURVE ((697064.616 245051.751,697064.773 245052.007,697067.63 245050.258,697067.473 245050.002,697064.616 245051.751)))"
    ]

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])

    feat = lyr.GetNextFeature()

    geom_field_values = [
        "MULTICURVE (COMPOUNDCURVE ((698298.028 246754.897,698295.899 246752.775,698293.113 246755.525,698295.243 246757.648)))"
    ]

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])

    lyr = ds.GetLayerByName("SURFC_TOP__Flaechenelement")
    assert lyr.GetFeatureCount() == 2, "feature count wrong."

    feat = lyr.GetNextFeature()

    geom_field_values = [
        "CURVEPOLYGON (COMPOUNDCURVE ((697064.616 245051.751,697064.773 245052.007,697067.63 245050.258,697067.473 245050.002,697064.616 245051.751))))"
    ]

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])

    feat = lyr.GetNextFeature()

    geom_field_values = [
        "CURVEPOLYGON (COMPOUNDCURVE ((698298.028 246754.897,698295.899 246752.775,698293.113 246755.525,698295.243 246757.648),(698295.243 246757.648,698298.028 246754.897)))"
    ]

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])

    # Test assembling curves not in "right" order
    ds = ogr.Open("data/ili/surface_complex.itf,data/ili/surface.imd")
    lyr = ds.GetLayerByName("SURFC_TOP__SURFC_TBL")
    feat = lyr.GetNextFeature()

    geom_field_values = [
        "CURVEPOLYGON (COMPOUNDCURVE ((747955.101 265828.716,747954.975 265827.862),(747954.975 265827.862,747951.166 265828.348,747951.24 265833.326),(747951.24 265833.326,747955.101 265828.716)))"
    ]

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])


###############################################################################
# Ili1 Surface test with linear conversion.


def test_ogr_interlis1_13_linear():

    with gdal.config_option("OGR_STROKE_CURVE", "YES"):

        ds = ogr.Open("data/ili/surface.itf,data/ili/surface.imd")

        layers = [
            "SURFC_TOP__SURFC_TBL",
            "SURFC_TOP__SURFC_TBL_SHAPE",
            "SURFC_TOP__SURFC_TBL_TEXT_ID",
            "SURFC_TOP__SURFC_TBL_TEXT_ID_SHAPE",
            "SURFC_TOP__LineAttrib1",
            "SURFC_TOP__Flaechenelement",
            "SURFC_TOP__Flaechenelement_Geometrie",
        ]

        assert ds.GetLayerCount() == len(layers), "layer count wrong."

        for i in range(ds.GetLayerCount()):
            assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

        lyr = ds.GetLayerByName("SURFC_TOP__SURFC_TBL_SHAPE")

        assert lyr.GetFeatureCount() == 5, "feature count wrong."

        lyr = ds.GetLayerByName("SURFC_TOP__SURFC_TBL")

        assert lyr.GetFeatureCount() == 4, "feature count wrong."

        feat = lyr.GetNextFeature()

        field_values = ["103", 1, 3, 1, 23, 25000, 20060111]

        assert feat.GetFieldCount() == len(field_values), "field count wrong."

        for i in range(feat.GetFieldCount()):
            if feat.GetFieldAsString(i) != str(field_values[i]):
                feat.DumpReadable()
                print(feat.GetFieldAsString(i))
                pytest.fail("field value wrong.")

        geom_field_values = [
            "POLYGON ((598600.961 249487.174,598608.899 249538.768,598624.774 249594.331,598648.586 249630.05,598684.305 249661.8,598763.68 249685.612,598850.993 249685.612,598854.962 249618.143,598843.055 249550.675,598819.243 249514.956,598763.68 249479.237,598692.243 249447.487,598612.868 249427.643,598600.961 249487.174))"
        ]

        assert feat.GetGeomFieldCount() == len(
            geom_field_values
        ), "geom field count wrong."

        for i in range(feat.GetGeomFieldCount()):
            geom = feat.GetGeomFieldRef(i)
            ogrtest.check_feature_geometry(geom, geom_field_values[i])

        # --- test curved polygon

        geom_field_values = [
            "POLYGON ((598600.961 249487.174,598608.899 249538.768,598624.774 249594.331,598648.586 249630.05,598684.305 249661.8,598763.68 249685.612,598850.993 249685.612,598854.962 249618.143,598843.055 249550.675,598819.243 249514.956,598763.68 249479.237,598692.243 249447.487,598612.868 249427.643,598600.961 249487.174))"
        ]

        assert feat.GetGeomFieldCount() == len(
            geom_field_values
        ), "geom field count wrong."

        for i in range(feat.GetGeomFieldCount()):
            geom = feat.GetGeomFieldRef(i)
            ogrtest.check_feature_geometry(geom, geom_field_values[i])

        # --- test multi-ring polygon

        feat = lyr.GetNextFeature()
        feat = lyr.GetNextFeature()
        # field_values = ['106', 3, 3, 1, 23, 25000, 20060111]
        field_values = ["105", 3, 3, 1, 23, 25000, 20060111]

        assert feat.GetFieldCount() == len(field_values), "field count wrong."

        for i in range(feat.GetFieldCount()):
            if feat.GetFieldAsString(i) != str(field_values[i]):
                feat.DumpReadable()
                print(feat.GetFieldAsString(i))
                pytest.fail("field value wrong.")

        geom_field_values = [
            "POLYGON ((598330.204 249028.397,598344.756 249057.501,598390.838 249074.479,598422.367 249081.755,598459.96 249093.882,598493.915 249101.158,598523.019 249106.008,598563.038 249084.18,598589.716 249042.949,598603.056 249011.42,598607.907 248966.551,598577.59 248960.487,598493.915 248983.528,598424.793 248996.868,598359.308 249010.207,598330.204 249028.397))"
        ]

        assert feat.GetGeomFieldCount() == len(
            geom_field_values
        ), "geom field count wrong."

        for i in range(feat.GetGeomFieldCount()):
            geom = feat.GetGeomFieldRef(i)
            ogrtest.check_feature_geometry(geom, geom_field_values[i])

        lyr = ds.GetLayerByName("SURFC_TOP__Flaechenelement_Geometrie")
        assert lyr.GetFeatureCount() == 3, "feature count wrong."

        feat = lyr.GetNextFeature()

        geom_field_values = [
            "MULTICURVE (COMPOUNDCURVE ((697064.616 245051.751,697064.773 245052.007,697067.63 245050.258,697067.473 245050.002,697064.616 245051.751)))"
        ]

        for i in range(feat.GetGeomFieldCount()):
            geom = feat.GetGeomFieldRef(i)
            ogrtest.check_feature_geometry(geom, geom_field_values[i])

        feat = lyr.GetNextFeature()

        geom_field_values = [
            "MULTICURVE (COMPOUNDCURVE ((698298.028 246754.897,698295.899 246752.775,698293.113 246755.525,698295.243 246757.648)))"
        ]

        for i in range(feat.GetGeomFieldCount()):
            geom = feat.GetGeomFieldRef(i)
            ogrtest.check_feature_geometry(geom, geom_field_values[i])

        lyr = ds.GetLayerByName("SURFC_TOP__Flaechenelement")
        assert lyr.GetFeatureCount() == 2, "feature count wrong."

        feat = lyr.GetNextFeature()

        geom_field_values = [
            "POLYGON ((697064.616 245051.751,697064.773 245052.007,697067.63 245050.258,697067.473 245050.002,697064.616 245051.751))"
        ]

        for i in range(feat.GetGeomFieldCount()):
            geom = feat.GetGeomFieldRef(i)
            ogrtest.check_feature_geometry(geom, geom_field_values[i])

        feat = lyr.GetNextFeature()

        geom_field_values = [
            "POLYGON ((698298.028 246754.897,698295.899 246752.775,698293.113 246755.525,698295.243 246757.648,698298.028 246754.897))"
        ]

        for i in range(feat.GetGeomFieldCount()):
            geom = feat.GetGeomFieldRef(i)
            ogrtest.check_feature_geometry(geom, geom_field_values[i])


###############################################################################
# Write Ili1 Arcs.


def test_ogr_interlis1_14():

    ds = ogr.Open("data/ili/Beispiel.itf,data/ili/Beispiel.imd")
    lyr = ds.GetLayerByName("Bodenbedeckung__Strasse")
    feat = lyr.GetNextFeature()

    driver = ogr.GetDriverByName("Interlis 1")
    outfile = "tmp/interlis1_14.itf"
    dst_ds = driver.CreateDataSource(outfile + ",data/ili/Beispiel.imd")

    dst_lyr = dst_ds.CreateLayer("Bodenbedeckung__Strasse", None, ogr.wkbMultiCurve)

    layer_defn = lyr.GetLayerDefn()
    for i in range(layer_defn.GetFieldCount()):
        dst_lyr.CreateField(layer_defn.GetFieldDefn(i))
    dst_feat = ogr.Feature(feature_def=dst_lyr.GetLayerDefn())
    dst_feat.SetFrom(feat)
    dst_lyr.CreateFeature(dst_feat)

    dst_ds = None

    with open(outfile) as f:
        itf = f.read()
        expected = """////
MTID INTERLIS1
MODL Beispiel
TABL Strasse
OBJE 3 100
STPT 190.26 208
ARCP 187 186
LIPT 173.1 171
LIPT 141.08 152.94
ELIN
ETAB
ETOP
EMOD
ENDE
"""
        assert expected in itf, "Interlis output doesn't match."


###############################################################################
# Reading Ili2 without model


def test_ogr_interlis2_1():

    ds = ogr.Open("data/ili/RoadsExdm2ien.xml")
    assert ds is not None

    layers = [
        "RoadsExdm2ben.Roads.LandCover",
        "RoadsExdm2ben.Roads.Street",
        "RoadsExdm2ien.RoadsExtended.StreetAxis",
        "RoadsExdm2ben.Roads.StreetNamePosition",
        "RoadsExdm2ien.RoadsExtended.RoadSign",
    ]

    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"


###############################################################################
# Reading Ili2


def test_ogr_interlis2_2():

    ds = ogr.Open("data/ili/RoadsExdm2ien.xml,data/ili/RoadsExdm2ien.imd")
    assert ds is not None

    layers = [
        "RoadsExdm2ben.Roads.LAttrs",
        "RoadsExdm2ben.Roads.LandCover",
        "RoadsExdm2ben.Roads.Street",
        "RoadsExdm2ben.Roads.StreetNamePosition",
        "RoadsExdm2ien.RoadsExtended.StreetAxis",
        "RoadsExdm2ien.RoadsExtended.RoadSign",
    ]

    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

    lyr = ds.GetLayerByName("RoadsExdm2ien.RoadsExtended.RoadSign")
    assert lyr.GetFeatureCount() == 4, "feature count wrong."

    feat = lyr.GetNextFeature()

    field_values = [501, "prohibition.noparking"]

    if feat.GetFieldCount() != len(field_values):
        feat.DumpReadable()
        pytest.fail("field count wrong.")

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
            feat.DumpReadable()
            print(feat.GetFieldAsString(i))
            pytest.fail("field value wrong.")

    geom = feat.GetGeometryRef()
    assert geom.GetCoordinateDimension() == 2, "dimension wrong."

    geom_field_values = ["POINT (69.389 92.056)"]

    assert feat.GetGeomFieldCount() == len(geom_field_values), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])

    lyr = ds.GetLayerByName("RoadsExdm2ien.RoadsExtended.StreetAxis")
    assert lyr.GetFeatureCount() == 7, "feature count wrong."

    feat = lyr.GetNextFeature()

    geom_field_values = ["LINESTRING (55.6 37.649,15.573 25.785)"]

    assert feat.GetGeomFieldCount() == len(geom_field_values), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])

    lyr = ds.GetLayerByName("RoadsExdm2ben.Roads.LandCover")
    assert lyr.GetFeatureCount() == 12, "feature count wrong."

    feat = lyr.GetNextFeature()

    geom_field_values = [
        "POLYGON ((39.038 60.315,41.2 59.302,43.362 60.315,44.713 66.268,45.794 67.662,48.766 67.408,53.36 64.115,56.197 62.595,57.818 63.862,58.899 68.928,55.927 72.348,47.955 75.515,42.281 75.388,39.308 73.235,36.741 69.688,35.525 66.268,35.661 63.735,37.957 61.455,39.038 60.315))"
    ]

    assert feat.GetGeomFieldCount() == len(geom_field_values), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])


###############################################################################
# Write Ili2 transfer file.


def test_ogr_interlis2_3():

    ds = ogr.Open("data/ili/RoadsExdm2ien.xml,data/ili/RoadsExdm2ien.imd")

    lyr = ds.GetLayerByName("RoadsExdm2ien.RoadsExtended.RoadSign")
    feat = lyr.GetNextFeature()

    driver = ogr.GetDriverByName("Interlis 2")
    outfile = "tmp/interlis2_3.xtf"
    dst_ds = driver.CreateDataSource(outfile + ",data/ili/RoadsExdm2ien.imd")

    dst_lyr = dst_ds.CreateLayer("RoadsExdm2ien.RoadsExtended.RoadSign")

    dst_feat = ogr.Feature(feature_def=dst_lyr.GetLayerDefn())
    dst_feat.SetFrom(feat)
    dst_lyr.CreateFeature(dst_feat)

    lyr = ds.GetLayerByName("RoadsExdm2ben.Roads.LandCover")
    feat = lyr.GetNextFeature()

    dst_lyr = dst_ds.CreateLayer("RoadsExdm2ben.Roads.LandCover")

    dst_feat = ogr.Feature(feature_def=dst_lyr.GetLayerDefn())
    dst_feat.SetFrom(feat)
    dst_lyr.CreateFeature(dst_feat)

    dst_ds = None

    with open(outfile) as f:
        xtf = f.read()
        expected = """<?xml version="1.0" encoding="utf-8" ?>
<TRANSFER xmlns="http://www.interlis.ch/INTERLIS2.3">
<HEADERSECTION SENDER="OGR/GDAL"""
        assert expected in xtf, "Interlis output doesn't match."
        expected = """<MODELS>
<MODEL NAME="RoadsExdm2ben" URI="http://www.interlis.ch/models" VERSION="2005-06-16"/>
<MODEL NAME="RoadsExdm2ien" URI="http://www.interlis.ch/models" VERSION="2005-06-16"/>
</MODELS>
</HEADERSECTION>
<DATASECTION>
<RoadsExdm2ien.RoadsExtended BID="RoadsExdm2ien.RoadsExtended">
<RoadsExdm2ien.RoadsExtended.RoadSign TID="501">
<Position>
<COORD><C1>69.389</C1><C2>92.056</C2></COORD>
</Position>
<Type>prohibition.noparking</Type>
</RoadsExdm2ien.RoadsExtended.RoadSign>
<RoadsExdm2ben.Roads.LandCover TID="16">
<Geometry>
<SURFACE>
<BOUNDARY>
<POLYLINE>
<COORD><C1>39.038</C1><C2>60.315</C2></COORD>
<COORD><C1>41.2</C1><C2>59.302</C2></COORD>
<COORD><C1>43.362</C1><C2>60.315</C2></COORD>
<COORD><C1>44.713</C1><C2>66.268</C2></COORD>
<COORD><C1>45.794</C1><C2>67.66200000000001</C2></COORD>
<COORD><C1>48.766</C1><C2>67.408</C2></COORD>
<COORD><C1>53.36</C1><C2>64.11499999999999</C2></COORD>
<COORD><C1>56.197</C1><C2>62.595</C2></COORD>
<COORD><C1>57.818</C1><C2>63.862</C2></COORD>
<COORD><C1>58.899</C1><C2>68.928</C2></COORD>
<COORD><C1>55.927</C1><C2>72.348</C2></COORD>
<COORD><C1>47.955</C1><C2>75.515</C2></COORD>
<COORD><C1>42.281</C1><C2>75.38800000000001</C2></COORD>
<COORD><C1>39.308</C1><C2>73.235</C2></COORD>
<COORD><C1>36.741</C1><C2>69.688</C2></COORD>
<COORD><C1>35.525</C1><C2>66.268</C2></COORD>
<COORD><C1>35.661</C1><C2>63.735</C2></COORD>
<COORD><C1>37.957</C1><C2>61.455</C2></COORD>
<COORD><C1>39.038</C1><C2>60.315</C2></COORD>
</POLYLINE>
</BOUNDARY>
</SURFACE>
</Geometry>
<Type>water</Type>
</RoadsExdm2ben.Roads.LandCover>
</RoadsExdm2ien.RoadsExtended>
</DATASECTION>
</TRANSFER>"""
        expected = expected.replace(".11499999999999", ".115")
        xtf = xtf.replace(".11499999999999", ".115")
        assert expected in xtf, "Interlis output doesn't match."


###############################################################################
# Ili2 Oereb model


def test_ogr_interlis2_4():

    ds = ogr.Open(
        "data/ili/ch.bazl.sicherheitszonenplan.oereb_20131118.xtf,data/ili/ch.bazl.sicherheitszonenplan.oereb_20131118.imd"
    )
    assert ds is not None

    layers = [
        "chGeoId10.BFSNr_",
        "chGeoId10.Kanton_",
        "chGeoId10.KantonInklFl_",
        "CodeISO.CountryCodeISO_",
        "CodeISO.LanguageCodeISO_",
        "MultilingualText09.LocalizedMText",
        "MultilingualText09.LocalizedText",
        "MultilingualText09.LocalizedURI",
        "MultilingualText09.MultilingualText",
        "MultilingualText09.MultilingualURI",
        "OeREBKRM09.ArtikelNummer_",
        "OeREBKRM09.Datum_",
        "OeREBKRM09.Thema_",
        "OeREBKRM09.WebReferenz_",
        "OeREBKRM09.ArtikelInhaltMehrsprachig",
        "OeREBKRM09vs.Vorschriften.Amt",
        "OeREBKRM09vs.Vorschriften.Artikel",
        "OeREBKRM09vs.Vorschriften.Rechtsvorschrift",
        "OeREBKRM09vs.Vorschriften.HinweisWeitereDokumente",
        "OeREBKRM09trsfr.Transferstruktur.DarstellungsDienst",
        "OeREBKRM09trsfr.Transferstruktur.Eigentumsbeschraenkung",
        "OeREBKRM09trsfr.Transferstruktur.Geometrie",
        "OeREBKRM09trsfr.Transferstruktur.HinweisDefinition",
        "OeREBKRM09trsfr.Transferstruktur.GrundlageVerfeinerung",
        "OeREBKRM09trsfr.Transferstruktur.HinweisDefinitionDokument",
        "OeREBKRM09trsfr.Transferstruktur.HinweisVorschrift",
        "OeREBKRM09vs.Vorschriften.Dokument",
    ]

    assert ds.GetLayerCount() == len(layers), "layer count wrong."

    for i in range(ds.GetLayerCount()):
        assert ds.GetLayer(i).GetName() in layers, "Did not get right layers"

    lyr = ds.GetLayerByName("OeREBKRM09trsfr.Transferstruktur.Geometrie")
    assert lyr.GetFeatureCount() == 36, "feature count wrong."

    feat = lyr.GetNextFeature()

    field_values = [
        "108-G-0002",
        "inKraft",
        "1975-06-27",
        "http://www.geocat.ch/geonetwork/srv/ger/metadata.show?fileIdentifier=ff218384-7251-4e68-85e7-c163dd366616",
        "108-Z-0002",
        "ch.admin.bazl",
    ]

    if feat.GetFieldCount() != len(field_values):
        feat.DumpReadable()
        pytest.fail("field count wrong.")

    for i in range(feat.GetFieldCount()):
        if feat.GetFieldAsString(i) != str(field_values[i]):
            feat.DumpReadable()
            print(feat.GetFieldAsString(i))
            pytest.fail("field value wrong.")

    geom_field_values = [
        None,
        None,
        "CURVEPOLYGON (COMPOUNDCURVE ((658593.928 254957.714,658511.628 254948.614,658418.028 254938.516,658106.426 254913.918,658192.222 254445.914,658771.228 254619.412,659667.232 254699.606,660369.238 254827.202,661016.442 255010.1,661279.644 255090.198,661866.648 255138.094,661784.45 255601.798,661211.146 255432.8,660320.54 255352.806,659523.436 255206.71,658703.528 254966.814,658624.228 254961.014,658593.928 254957.714)))",
    ]
    assert feat.GetGeomFieldCount() == len(geom_field_values), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        val = geom_field_values[i]
        if val is None:
            assert geom is None
        else:
            ogrtest.check_feature_geometry(geom, val)


###############################################################################
# Check arc segmentation


def test_ogr_interlis_arc1():

    with gdal.config_option("OGR_STROKE_CURVE", "TRUE"):
        # gdal.SetConfigOption('OGR_ARC_STEPSIZE', '0.96')
        ds = ogr.Open("data/ili/Beispiel.itf,data/ili/Beispiel.imd")

    length_0_1_deg = 72.7181992353  # Line length with 0.1 degree segments

    # Read Area lines
    lyr = ds.GetLayerByName("Bodenbedeckung__BoFlaechen_Form")
    assert lyr.GetFeatureCount() == 4, "feature count wrong."

    # Get 3rd feature
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()

    geom = feat.GetGeometryRef()
    length = geom.Length()
    assert length == pytest.approx(length_0_1_deg, abs=0.001), (
        "Length() result wrong, got %g." % length
    )
    ogrtest.check_feature_geometry(
        geom,
        "MULTILINESTRING ((186.38 206.82,186.456637039593772 206.352374385077951,186.525513501820711 205.883542875348297,186.586610467027612 205.413634254015022,186.639911152487883 204.942777600139351,186.685400917071064 204.471102253228281,186.72306726518579 203.998737777656032,186.752899850289367 203.525813927088137,186.774890477674461 203.05246060886563,186.789033106731921 202.578807848280093,186.795323852631526 202.104985752892361,186.793760987376999 201.631124476756639,186.784344940257114 201.157354184717377,186.76707829777294 200.683805016597006,186.741965802895692 200.210607051507367,186.709014353764331 199.737890272066551,186.668233001808375 199.265784528717603,186.619632949244959 198.79441950407633,186.563227546008392 198.323924677271833,186.499032286083064 197.854429288403452,186.427064803254297 197.386062303034095,186.347344866240718 196.918952376778151,186.259894373302785 196.453227819933062,186.164737346189185 195.989016562249134,186.06189992355209 195.52644611780849,185.951410353787622 195.065643549954956,185.83329898723602 194.606735436434946,185.70759826788705 194.149847834589281,185.574342724430579 193.695106246756012,185.433568960797828 193.242635585782381,185.285315646106 192.792560140718678,185.129623504006275 192.345003542672174,184.966535301544326 191.900088730850229,184.796095837373258 191.457937918770739,184.618351929457248 191.018672560741805,184.433352402229474 190.582413318443258,184.241148073160701 190.149280027813774,184.04179173881127 189.71939166611989,183.835338160315558 189.292866319286958,183.621844048349857 188.869821149441094,183.401368047569093 188.450372362771276,183.173970720454207 188.03463517752968,182.939714530730242 187.622723792475853,182.698663826158707 187.214751355422749,182.450884820893378 186.810829932226596,182.196445577297681 186.411070475951391,181.935415987201822 186.015582796419011,181.667867752774299 185.624475530035824,181.3938743667604 185.237856109953981,181.113511092327798 184.855830736560137,180.826854942395556 184.478504348306132,180.53398465846837 184.105980592874374,180.234980689005141 183.73836179872157,179.929925167336449 183.375748946964421,179.618901889065427 183.018241643621849,179.301996289112111 182.665938092286495,178.979295418190276 182.318935067130923,178.650887918905909 181.977327886313986,178.316864001440962 181.64121038583832,177.97731541873506 181.310674893720687,177.632335441296135 180.985812204678922,177.282018831589056 180.666711555153569,176.9264618179877 180.353460598817009,176.565762068363227 180.046145382490039,176.200018663221243 179.744850322516839,175.829332068533375 179.449658181561944,175.453804108095909 179.160650045865594,175.073537935597386 178.877905303001143,174.688638006242371 178.601501620054449,174.299210048091453 178.331514922276227,173.905361033015623 178.068019372236421,173.507199147294131 177.811087349466078,173.104833761914023 177.560789430557605,172.698375402513165 177.317194369767066,172.28793571903222 177.080369080176723,171.873627455046488 176.850378615250492,171.455564416777605 176.627286151007837,171.033861441848018 176.411152968667096,170.60863436775486 176.202038437793476,170.18 176.0,140.69 156.63))",
    )

    line = geom.GetGeometryRef(0)
    points = line.GetPoints()
    assert len(points) == 81, "line point count wrong."

    # Get 4th feature
    feat = lyr.GetNextFeature()

    geom = feat.GetGeometryRef()
    length = geom.Length()
    assert length == pytest.approx(98.0243498288, abs=0.001), (
        "Length() result wrong, got %g." % length
    )
    ogrtest.check_feature_geometry(
        geom,
        "MULTILINESTRING ((186.38 206.82,194.26 208.19,194.360118941290381 207.583863387608318,194.450034548441351 206.976129417129414,194.52972141151983 206.366969832830335,194.599157011165488 205.756556781867374,194.658321725044487 205.145062765608571,194.707198833340925 204.532660590913991,194.745774523486006 203.919523321273999,194.774037894087058 203.305824227936654,194.791980957968889 202.691736740889581,194.799598644465704 202.077434399900056,194.796888800832647 201.463090805460496,194.783852192864259 200.848879569690297,194.760492504683469 200.234974267339538,194.72681633767931 199.621548386683543,194.682833208673372 199.00877528050475,194.628555547191326 198.396828117140075,194.56399869197773 197.785879831499159,194.489180886659568 197.176103076206317,194.40412327458759 196.567670172822517,194.308849892848201 195.960753063147394,194.203387665489544 195.355523260601302,194.087766395903571 194.752151801789267,193.962018758407766 194.150809198152245,193.826180289004668 193.55166538777118,193.680289375348337 192.954889687360236,193.524387245874067 192.360650744412823,193.358517958193232 191.769116489536799,193.182728386585921 191.180454089007952,192.997068208809623 190.594829897498101,192.80158989201297 190.012409411123343,192.596348677929171 189.433357220623265,192.381402567276353 188.857836964845745,192.156812303357555 188.28601128455918,191.922641354867636 187.718041776424769,191.678955898001703 187.15408894736899,191.425824797734066 186.594312169210724,191.163319588355051 186.038869633623079,190.891514453251119 185.48791830744446,190.610486203950103 184.941613888295223,190.320314258417028 184.400110760594515,190.021080618593231 183.863561951926357,189.71286984725154 183.332119089784072,189.395769044065645 182.805932358758554,189.069867821024644 182.285150458053948,188.735258277090878 181.769920559469,188.392034972181108 181.260388265836781,188.040294900434645 180.756697569857323,187.680137462819374 180.258990813403187,187.311664439032 179.767408647305245,186.934979958736193 179.282089991582296,186.550190472116782 178.803171996223654,186.157404719844607 178.330790002393741,185.756733702299215 177.865077504182381,185.348290648216761 177.406166110929888,184.932190982704896 176.954185509959615,184.508552294610098 176.50926343000171,184.077494303273824 176.071525605046162,183.639138824742957 175.641095738836128,183.193609737303603 175.218095469892404,182.741032946476452 174.802644337149076,182.281536349473726 174.394859746185801,181.815249799001293 173.994856936020341,181.34230506662422 173.60274894655592,180.86283580545566 173.21864658665433,180.376977512460115 172.84265840277655,179.884867490109144 172.47489064835824,179.38664480760778 172.115447253752762,178.882450261575258 171.764429796865414,178.372426336281904 171.421937474442501,177.856717163354887 171.08806707407345,177.335468481061952 170.762912946782279,176.808827593129507 170.446566980404867,176.276943327102316 170.13911857360651,175.739965992295737 169.840654610641622,175.198047337304331 169.551259436742839,174.651340507162928 169.271014834364934,174.1 169.0,145.08 149.94,140.69 156.63))",
    )

    line = geom.GetGeometryRef(0)
    points = line.GetPoints()
    assert len(points) == 81, "line point count wrong."


###############################################################################
# Check polyline with arc


def test_ogr_interlis_arc2():

    ds = ogr.Open("data/ili/Beispiel.itf,data/ili/Beispiel.imd")

    lyr = ds.GetLayerByName("Bodenbedeckung__Strasse")
    assert lyr.GetFeatureCount() == 1, "feature count wrong."

    feat = lyr.GetNextFeature()
    geom_field_values = [
        "MULTICURVE (COMPOUNDCURVE (CIRCULARSTRING (190.26 208.0,187 186,173.1 171.0),(173.1 171.0,141.08 152.94)))"
    ]

    assert feat.GetGeomFieldCount() == len(geom_field_values), "geom field count wrong."

    for i in range(feat.GetGeomFieldCount()):
        geom = feat.GetGeomFieldRef(i)
        ogrtest.check_feature_geometry(geom, geom_field_values[i])
