#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector compare' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr, osr


def test_gdalalg_vector_compare_same_file():

    with gdal.alg.vector.compare(
        input="../ogr/data/poly.shp",
        reference="../ogr/data/poly.shp",
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_vector_compare_same_dataset():

    ds = ogr.Open("../ogr/data/poly.shp")
    with gdal.alg.vector.compare(
        input=ds,
        reference=ds,
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_vector_compare_same_file_skip_binary_progress():

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.alg.vector.compare(
        input="../ogr/data/poly.shp",
        reference="../ogr/data/poly.shp",
        skip_binary=True,
        progress=my_progress,
    ) as alg:
        assert alg["output-string"] == ""

    assert tab_pct[0] == 1


@pytest.mark.require_driver("OSM")
def test_gdalalg_vector_compare_same_file_pbf_skip_binary_no_progress():

    with gdal.alg.vector.compare(
        input="../ogr/data/osm/test.pbf",
        reference="../ogr/data/osm/test.pbf",
        skip_binary=True,
    ) as alg:
        assert alg["output-string"] == ""


@pytest.mark.require_driver("OSM")
def test_gdalalg_vector_compare_same_file_pbf_skip_binary_progress():

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.alg.vector.compare(
        input="../ogr/data/osm/test.pbf",
        reference="../ogr/data/osm/test.pbf",
        skip_binary=True,
        progress=my_progress,
    ) as alg:
        assert alg["output-string"] == ""

    assert tab_pct[0] == 1


@pytest.mark.require_driver("OSM")
def test_gdalalg_vector_compare_different_file_pbf():

    with gdal.alg.vector.compare(
        input="../ogr/data/osm/test.pbf",
        reference="../ogr/data/osm/test_json.pbf",
    ) as alg:
        assert (
            alg["output-string"]
            == 'Reference file \'../ogr/data/osm/test_json.pbf\' has size 204 bytes, whereas input file has size 565 bytes.\nLayer points: Feature at index 0 has value \'\' for field name in reference layer, whereas it is \'Some interesting point\' in input layer\nLayer points: Feature at index 0 has value \'"foo"=>"x\'\\\\\\"\t\n\ry"\' for field other_tags in reference layer, whereas it is \'"foo"=>"bar","bar"=>"baz"\' in input layer\nLayer lines: Input layer has 2 feature(s), whereas reference layer has 0\nLayer multilinestrings: Input layer has 1 feature(s), whereas reference layer has 0\nLayer multipolygons: Input layer has 3 feature(s), whereas reference layer has 0\nLayer other_relations: Input layer has 1 feature(s), whereas reference layer has 0\n'
        )

    with gdal.alg.vector.compare(
        input="../ogr/data/osm/test_json.pbf",
        reference="../ogr/data/osm/test.pbf",
    ) as alg:
        assert (
            alg["output-string"]
            == 'Reference file \'../ogr/data/osm/test.pbf\' has size 565 bytes, whereas input file has size 204 bytes.\nLayer points: Feature at index 0 has value \'Some interesting point\' for field name in reference layer, whereas it is \'\' in input layer\nLayer points: Feature at index 0 has value \'"foo"=>"bar","bar"=>"baz"\' for field other_tags in reference layer, whereas it is \'"foo"=>"x\'\\\\\\"\t\n\ry"\' in input layer\nLayer lines: Reference layer has 2 feature(s), whereas input layer has 0\nLayer multilinestrings: Reference layer has 1 feature(s), whereas input layer has 0\nLayer multipolygons: Reference layer has 3 feature(s), whereas input layer has 0\nLayer other_relations: Reference layer has 1 feature(s), whereas input layer has 0\n'
        )


def test_gdalalg_vector_compare_interrupt_progress():

    def my_progress(pct, msg, user_data):
        return False

    with pytest.raises(Exception, match="Interrupted by user"):
        gdal.alg.vector.compare(
            input="../ogr/data/poly.shp",
            reference="../ogr/data/poly.shp",
            skip_binary=True,
            progress=my_progress,
        )


def test_gdalalg_vector_compare_interrupt_progress_empty_layer():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("test")

    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_ds.CreateLayer("test")

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        tab_pct[0] = pct
        return False

    with pytest.raises(Exception, match="Interrupted by user"):
        gdal.alg.vector.compare(
            input=src_ds,
            reference=src2_ds,
            skip_binary=True,
            progress=my_progress,
        )
    assert tab_pct[0] == 1

    tab_pct[0] = 0
    with pytest.raises(Exception, match="Interrupted by user"):
        gdal.alg.vector.compare(
            input=src_ds,
            reference=src2_ds,
            layer="test",
            skip_binary=True,
            progress=my_progress,
        )
    assert tab_pct[0] == 1


def test_gdalalg_vector_compare_interrupt_progress_non_empty_layer():

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        tab_pct[0] = pct
        return False

    with pytest.raises(Exception, match="Interrupted by user"):
        gdal.alg.vector.compare(
            input="../ogr/data/poly.shp",
            reference="../ogr/data/poly.shp",
            skip_binary=True,
            progress=my_progress,
        )
    assert tab_pct[0] == 0

    with pytest.raises(Exception, match="Interrupted by user"):
        gdal.alg.vector.compare(
            input="../ogr/data/poly.shp",
            reference="../ogr/data/poly.shp",
            layer="poly",
            skip_binary=True,
            progress=my_progress,
        )
    assert tab_pct[0] == 0


def test_gdalalg_vector_compare_two_identical_shapefiles(tmp_vsimem):

    gdal.alg.vector.convert(input="../ogr/data/poly.shp", output=tmp_vsimem / "out.shp")
    with gdal.alg.vector.compare(
        input=tmp_vsimem / "out.shp",
        reference="../ogr/data/poly.shp",
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_vector_compare_in_pipeline():

    with gdal.alg.vector.pipeline(
        pipeline="read ../ogr/data/poly.shp ! compare ../ogr/data/poly.shp"
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_vector_compare_missing_layer():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert alg["output-string"] == ""

    src1_ds.CreateLayer("foo")

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference dataset has 0 layer(s), whereas input dataset has 1\nLayer foo present in input dataset is absent from reference dataset\n"
        )

    with gdal.alg.vector.compare(
        input=src2_ds,
        reference=src1_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Reference dataset has 1 layer(s), whereas input dataset has 0\nLayer foo present in reference dataset is absent from input dataset\n"
        )

    with pytest.raises(
        Exception,
        match="Layer foo present in input dataset is absent from reference dataset",
    ):
        gdal.alg.vector.compare(input=src1_ds, reference=src2_ds, layer="foo")

    with pytest.raises(Exception, match="Cannot find source layer 'foo'"):
        gdal.alg.vector.compare(input=src2_ds, reference=src1_ds, layer="foo")


def test_gdalalg_vector_compare_different_dataset_metadata():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_ds.SetMetadata({"FOO": "BAR"})
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Input metadata (dataset default metadata domain) contains key 'FOO' but reference metadata does not.\n"
        )

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
        skip_all_optional=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_vector_compare_different_layer_metadata():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.SetMetadata({"FOO": "BAR"})
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_ds.CreateLayer("test")

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Input metadata (layer test default metadata domain) contains key 'FOO' but reference metadata does not.\n"
        )

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
        skip_metadata=True,
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_vector_compare_different_field_count():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_ds.CreateLayer("test")

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Reference layer has 0 attribute field(s), whereas input layer has 1\nLayer test: Input layer has field foo, which is absent in reference layer\n"
        )

    with gdal.alg.vector.compare(
        input=src2_ds,
        reference=src1_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Reference layer has 1 attribute field(s), whereas input layer has 0\nLayer test: Reference layer has field foo, which is absent in input layer\n"
        )


def test_gdalalg_vector_compare_different_field_type():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    src2_lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTInteger))

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has type Integer in reference layer, but String in input layer\n"
        )


def test_gdalalg_vector_compare_different_field_subtype():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetSubType(ogr.OFSTJSON)
    src2_lyr.CreateField(fld_defn)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has subtype JSON in reference layer, but None in input layer\n"
        )


def test_gdalalg_vector_compare_different_field_width():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetWidth(10)
    src2_lyr.CreateField(fld_defn)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has width 10 in reference layer, but 0 in input layer\n"
        )


def test_gdalalg_vector_compare_different_field_precision():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetPrecision(10)
    src2_lyr.CreateField(fld_defn)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has precision 10 in reference layer, but 0 in input layer\n"
        )


def test_gdalalg_vector_compare_different_field_nullable():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetNullable(False)
    src2_lyr.CreateField(fld_defn)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has nullable=0 in reference layer, but nullable=1 in input layer\n"
        )


def test_gdalalg_vector_compare_different_field_unique():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetUnique(True)
    src2_lyr.CreateField(fld_defn)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has unique constraint=1 in reference layer, but unique constraint=0 in input layer\n"
        )


def test_gdalalg_vector_compare_different_field_generated():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetGenerated(True)
    src2_lyr.CreateField(fld_defn)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has generated status=1 in reference layer, but generated status=0 in input layer\n"
        )


def test_gdalalg_vector_compare_different_field_comment():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetComment("my comment")
    src2_lyr.CreateField(fld_defn)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has 'my comment' as comment in reference layer, but '' in input layer\n"
        )


def test_gdalalg_vector_compare_different_field_alternative_name():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetAlternativeName("alias")
    src2_lyr.CreateField(fld_defn)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has 'alias' as alternative name in reference layer, but '' in input layer\n"
        )


def test_gdalalg_vector_compare_different_field_default():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    fld_defn = ogr.FieldDefn("foo")
    fld_defn.SetDefault("CURRENT_TIMESTAMP")
    src2_lyr.CreateField(fld_defn)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src1_ds,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.alg.vector.compare(
        input=src2_ds,
        reference=src2_ds,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has 'CURRENT_TIMESTAMP' as default in reference layer, but '(null)' in input layer\n"
        )

    with gdal.alg.vector.compare(
        input=src2_ds,
        reference=src1_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has '(null)' as default in reference layer, but 'CURRENT_TIMESTAMP' in input layer\n"
        )


def test_gdalalg_vector_compare_different_geometry_field_count():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_ds.CreateLayer("test")
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_ds.CreateLayer("test", geom_type=ogr.wkbNone)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Reference layer has 0 geometry field(s), whereas input layer has 1\nLayer test: Input layer has geometry field '', which is absent in reference layer\n"
        )

    with gdal.alg.vector.compare(
        input=src2_ds,
        reference=src1_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Reference layer has 1 geometry field(s), whereas input layer has 0\nLayer test: Reference layer has geometry field '', which is absent in input layer\n"
        )


def test_gdalalg_vector_compare_different_geometry_field_type():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_ds.CreateLayer("test", geom_type=ogr.wkbLineString)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Geometry field '' has geometry type Line String in reference layer, but Point in input layer\n"
        )


def test_gdalalg_vector_compare_different_geometry_field_nullable():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_ds.CreateLayer("test")
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test", geom_type=ogr.wkbNone)
    geom_field_defn = ogr.GeomFieldDefn("", ogr.wkbUnknown)
    geom_field_defn.SetNullable(False)
    src2_lyr.CreateGeomField(geom_field_defn)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Geometry field '' has nullable=0 in reference layer, but nullable=1 in input layer\n"
        )


def test_gdalalg_vector_compare_different_crs():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_ds.CreateLayer("test", srs=osr.SpatialReference(epsg=4326))

    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_ds.CreateLayer("test", srs=osr.SpatialReference(epsg=4258))

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src1_ds,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert alg["output-string"].startswith(
            "Layer test: Geometry field '' has different CRS in reference and input layers"
        )

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
        skip_crs=True,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
        skip_all_optional=True,
    ) as alg:
        assert alg["output-string"] == ""

    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_ds.CreateLayer("test")

    with gdal.alg.vector.compare(
        input=src2_ds,
        reference=src2_ds,
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert alg["output-string"].startswith(
            "Layer test: Geometry field '' has different CRS in reference and input layers"
        )

    with gdal.alg.vector.compare(
        input=src2_ds,
        reference=src1_ds,
    ) as alg:
        assert alg["output-string"].startswith(
            "Layer test: Geometry field '' has different CRS in reference and input layers"
        )


def test_gdalalg_vector_compare_different_feature_count():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_ds.CreateLayer("test")
    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    src2_lyr.CreateFeature(ogr.Feature(src2_lyr.GetLayerDefn()))

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Reference layer has 1 feature(s), whereas input layer has 0\n"
        )

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
        progress=my_progress,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Reference layer has 1 feature(s), whereas input layer has 0\n"
        )


def test_gdalalg_vector_compare_different_feature_id():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    f = ogr.Feature(src1_lyr.GetLayerDefn())
    src1_lyr.CreateFeature(f)

    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    f = ogr.Feature(src2_lyr.GetLayerDefn())
    f.SetFID(10)
    src2_lyr.CreateFeature(f)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Feature at index 0 has feature id 10 in reference layer, whereas it is 0 in input layer\n"
        )

    with gdal.alg.vector.compare(
        input=src1_ds, reference=src2_ds, skip_fid=True
    ) as alg:
        assert alg["output-string"] == ""


def test_gdalalg_vector_compare_different_feature_field_content():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("str"))
    f = ogr.Feature(src1_lyr.GetLayerDefn())
    f["str"] = "foo"
    src1_lyr.CreateFeature(f)

    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    src2_lyr.CreateField(ogr.FieldDefn("str"))
    f = ogr.Feature(src2_lyr.GetLayerDefn())
    src2_lyr.CreateFeature(f)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Feature at index 0 has value '' for field str in reference layer, whereas it is 'foo' in input layer\n"
        )


def test_gdalalg_vector_compare_same_field_content_but_not_same_time():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    src1_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(src1_lyr.GetLayerDefn())
    f["foo"] = "1"
    src1_lyr.CreateFeature(f)

    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    src2_lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTInteger))
    f = ogr.Feature(src2_lyr.GetLayerDefn())
    f["foo"] = 1
    src2_lyr.CreateFeature(f)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Field 'foo' has type Integer in reference layer, but String in input layer\n"
        )


def test_gdalalg_vector_compare_different_feature_geometry():

    src1_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src1_lyr = src1_ds.CreateLayer("test")
    f = ogr.Feature(src1_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON EMPTY"))
    src1_lyr.CreateFeature(f)

    src2_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src2_lyr = src2_ds.CreateLayer("test")
    f = ogr.Feature(src2_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("MULTIPOLYGON EMPTY"))
    src2_lyr.CreateFeature(f)

    src3_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src3_lyr = src3_ds.CreateLayer("test")
    f = ogr.Feature(src3_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT EMPTY"))
    src3_lyr.CreateFeature(f)

    src4_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src4_lyr = src4_ds.CreateLayer("test")
    f = ogr.Feature(src4_lyr.GetLayerDefn())
    src4_lyr.CreateFeature(f)

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src2_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Feature at index 0 has value MULTIPOLYGON EMPTY for geometry field '' in reference layer, whereas it is POLYGON EMPTY in input layer\n"
        )

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src4_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Feature at index 0 has value null for geometry field '' in reference layer, whereas it is POLYGON EMPTY in input layer\n"
        )

    with gdal.alg.vector.compare(
        input=src4_ds,
        reference=src1_ds,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Feature at index 0 has value POLYGON EMPTY for geometry field '' in reference layer, whereas it is null in input layer\n"
        )

    with gdal.alg.vector.compare(
        input=src1_ds, reference=src2_ds, lax_geometry=True
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.alg.vector.compare(
        input=src2_ds, reference=src1_ds, lax_geometry=True
    ) as alg:
        assert alg["output-string"] == ""

    with gdal.alg.vector.compare(
        input=src1_ds, reference=src3_ds, lax_geometry=True
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Feature at index 0 has value POINT EMPTY for geometry field '' in reference layer, whereas it is POLYGON EMPTY in input layer\n"
        )

    with gdal.alg.vector.compare(
        input=src1_ds,
        reference=src4_ds,
        lax_geometry=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Feature at index 0 has value null for geometry field '' in reference layer, whereas it is POLYGON EMPTY in input layer\n"
        )

    with gdal.alg.vector.compare(
        input=src4_ds,
        reference=src1_ds,
        lax_geometry=True,
    ) as alg:
        assert (
            alg["output-string"]
            == "Layer test: Feature at index 0 has value POLYGON EMPTY for geometry field '' in reference layer, whereas it is null in input layer\n"
        )
