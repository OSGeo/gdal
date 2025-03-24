#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
################################################################################
#  Project: OGR NextGIS Web Driver
#  Purpose: Tests OGR NGW Driver capabilities
#  Author: Dmitry Baryshnikov, polimax@mail.ru
#  Language: Python
################################################################################
#  The MIT License (MIT)
#
#  Copyright (c) 2018-2025, NextGIS <info@nextgis.com>
#
# SPDX-License-Identifier: MIT
################################################################################

import os
import sys

sys.path.append("../pymod")

import random
import time

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = [
    pytest.mark.require_driver("NGW"),
    pytest.mark.random_order(disabled=True),
    pytest.mark.skipif(
        "CI" in os.environ,
        reason="NGW tests are flaky. See https://github.com/OSGeo/gdal/issues/4453",
    ),
]

NET_TIMEOUT = 130
NET_MAX_RETRY = 5
NET_RETRY_DELAY = 2


def check_availability(url):
    # Check NGW availability
    version_url = url + "/api/component/pyramid/pkg_version"
    if gdaltest.gdalurlopen(version_url, timeout=NET_TIMEOUT) is None:
        return False
    return True


def get_new_name():
    return "gdaltest_group_" + str(int(time.time())) + "_" + str(random.randint(10, 99))


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    gdaltest.ngw_test_server = "https://sandbox.nextgis.com"

    if check_availability(gdaltest.ngw_test_server) == False:
        pytest.skip()

    yield

    if gdaltest.group_id is not None:
        delete_url = (
            "NGW:" + gdaltest.ngw_test_server + "/resource/" + gdaltest.group_id
        )

        gdaltest.ngw_layer = None
        gdaltest.ngw_ds = None

        assert gdal.GetDriverByName("NGW").Delete(delete_url) == gdal.CE_None, (
            "Failed to delete datasource " + delete_url + "."
        )

    gdaltest.ngw_ds = None


###############################################################################
# Check create datasource.


@pytest.mark.slow()
def test_ogr_ngw_2():

    create_url = "NGW:" + gdaltest.ngw_test_server + "/resource/0/" + get_new_name()
    gdal.ErrorReset()
    with gdal.quiet_errors():
        gdaltest.ngw_ds = gdal.GetDriverByName("NGW").Create(
            create_url,
            0,
            0,
            0,
            gdal.GDT_Unknown,
            options=[
                "DESCRIPTION=GDAL Test group",
                f"TIMEOUT={NET_TIMEOUT}",
                f"MAX_RETRY={NET_MAX_RETRY}",
                f"RETRY_DELAY={NET_RETRY_DELAY}",
            ],
        )

    assert gdaltest.ngw_ds is not None, "Create datasource failed."
    assert (
        gdaltest.ngw_ds.GetMetadataItem("description", "") == "GDAL Test group"
    ), "Did not get expected datasource description."

    assert (
        int(gdaltest.ngw_ds.GetMetadataItem("id", "")) > 0
    ), "Did not get expected datasource identifier."

    gdaltest.group_id = gdaltest.ngw_ds.GetMetadataItem("id", "")


###############################################################################
# Check rename datasource.


@pytest.mark.slow()
def test_ogr_ngw_3():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    new_name = get_new_name() + "_2"
    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem("id", "")
    rename_url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + ds_resource_id

    assert (
        gdal.GetDriverByName("NGW").Rename(new_name, rename_url) == gdal.CE_None
    ), "Rename datasource failed."


###############################################################################
# Check datasource metadata.


@pytest.mark.slow()
def test_ogr_ngw_4():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem("id", "")
    gdaltest.ngw_ds.SetMetadataItem("test_int.d", "777", "NGW")
    gdaltest.ngw_ds.SetMetadataItem("test_float.f", "777.555", "NGW")
    gdaltest.ngw_ds.SetMetadataItem("test_string", "metadata test", "NGW")

    gdaltest.ngw_ds = None
    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + ds_resource_id
    gdaltest.ngw_ds = gdal.OpenEx(
        url,
        gdal.OF_UPDATE,
        open_options=[
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )
    assert gdaltest.ngw_ds is not None, "Open datasource failed."

    md_item = gdaltest.ngw_ds.GetMetadataItem("test_int.d", "NGW")
    assert (
        md_item == "777"
    ), f"Did not get expected datasource metadata item. test_int.d is equal {md_item}, but should 777."

    md_item = gdaltest.ngw_ds.GetMetadataItem("test_float.f", "NGW")
    assert float(md_item) == pytest.approx(
        777.555, abs=0.00001
    ), f"Did not get expected datasource metadata item. test_float.f is equal {md_item}, but should 777.555."

    md_item = gdaltest.ngw_ds.GetMetadataItem("test_string", "NGW")
    assert (
        md_item == "metadata test"
    ), f"Did not get expected datasource metadata item. test_string is equal {md_item}, but should 'metadata test'."

    resource_type = gdaltest.ngw_ds.GetMetadataItem("resource_type", "")
    assert (
        resource_type is not None
    ), "Did not get expected datasource metadata item. Resourse type should be present."


def create_fields(lyr):
    fld_defn = ogr.FieldDefn("STRFIELD", ogr.OFTString)
    fld_defn.SetAlternativeName("String field test")
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("DECFIELD", ogr.OFTInteger)
    fld_defn.SetAlternativeName("Integer field test")
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("BIGDECFIELD", ogr.OFTInteger64)
    fld_defn.SetAlternativeName("Integer64 field test")
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("REALFIELD", ogr.OFTReal)
    fld_defn.SetAlternativeName("Real field test")
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("DATEFIELD", ogr.OFTDate)
    fld_defn.SetAlternativeName("Date field test")
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("TIMEFIELD", ogr.OFTTime)
    fld_defn.SetAlternativeName("Time field test")
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("DATETIMEFLD", ogr.OFTDateTime)
    fld_defn.SetAlternativeName("Date & time field test")
    lyr.CreateField(fld_defn)


def fill_fields(f):
    f.SetField("STRFIELD", "fo_o")
    f.SetField("DECFIELD", 123)
    f.SetField("BIGDECFIELD", 12345678901234)
    f.SetField("REALFIELD", 1.23)
    f.SetField("DATETIMEFLD", "2014/12/04 12:34:56")


def fill_fields2(f):
    f.SetField("STRFIELD", "русский")
    f.SetField("DECFIELD", 321)
    f.SetField("BIGDECFIELD", 32145678901234)
    f.SetField("REALFIELD", 21.32)
    f.SetField("DATETIMEFLD", "2019/12/31 21:43:56")


def add_metadata(lyr):
    lyr.SetMetadataItem("test_int.d", "777", "NGW")
    lyr.SetMetadataItem("test_float.f", "777,555", "NGW")
    lyr.SetMetadataItem("test_string", "metadata test", "NGW")


###############################################################################
# Check create vector layers.


@pytest.mark.slow()  # 12s
def test_ogr_ngw_5():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3857)
    lyr = gdaltest.ngw_ds.CreateLayer(
        "test_pt_layer",
        srs=sr,
        geom_type=ogr.wkbMultiPoint,
        options=["OVERWRITE=YES", "DESCRIPTION=Test point layer"],
    )
    assert lyr is not None, "Create layer failed."

    create_fields(lyr)

    # Test duplicated names.
    gdal.ErrorReset()
    fld_defn = ogr.FieldDefn("STRFIELD", ogr.OFTString)
    try:
        assert lyr.CreateField(fld_defn) != 0, "Expected not to create duplicated field"
    except Exception:
        pass

    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer(
        "test_ln_layer",
        srs=sr,
        geom_type=ogr.wkbMultiLineString,
        options=["OVERWRITE=YES", "DESCRIPTION=Test line layer"],
    )
    assert lyr is not None, "Create layer failed."

    create_fields(lyr)
    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer(
        "test_pl_layer",
        srs=sr,
        geom_type=ogr.wkbMultiPolygon,
        options=["OVERWRITE=YES", "DESCRIPTION=Test polygon layer"],
    )
    assert lyr is not None, "Create layer failed."

    create_fields(lyr)
    add_metadata(lyr)

    # Test overwrite
    lyr = gdaltest.ngw_ds.CreateLayer(
        "test_pt_layer",
        srs=sr,
        geom_type=ogr.wkbPoint,
        options=["OVERWRITE=YES", "DESCRIPTION=Test point layer"],
    )
    assert lyr is not None, "Create layer failed."

    create_fields(lyr)
    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer(
        "test_ln_layer",
        srs=sr,
        geom_type=ogr.wkbLineString,
        options=["OVERWRITE=YES", "DESCRIPTION=Test line layer"],
    )
    assert lyr is not None, "Create layer failed."

    create_fields(lyr)
    add_metadata(lyr)

    lyr = gdaltest.ngw_ds.CreateLayer(
        "test_pl_layer",
        srs=sr,
        geom_type=ogr.wkbPolygon,
        options=["OVERWRITE=YES", "DESCRIPTION=Test polygon layer"],
    )
    assert lyr is not None, "Create layer failed."

    create_fields(lyr)
    add_metadata(lyr)

    # Test without overwrite
    gdal.ErrorReset()
    try:
        lyr = gdaltest.ngw_ds.CreateLayer(
            "test_pl_layer",
            srs=sr,
            geom_type=ogr.wkbMultiPolygon,
            options=["OVERWRITE=NO", "DESCRIPTION=Test polygon layer 1"],
        )
        assert lyr is None, "Create layer without overwrite should fail."
    except Exception:
        pass

    try:
        lyr = gdaltest.ngw_ds.CreateLayer(
            "test_pl_layer",
            srs=sr,
            geom_type=ogr.wkbMultiPolygon,
            options=["DESCRIPTION=Test point layer 1"],
        )
        assert lyr is None, "Create layer without overwrite should fail."
    except Exception:
        pass

    # Test geometry with Z
    lyr = gdaltest.ngw_ds.CreateLayer(
        "test_plz_layer",
        srs=sr,
        geom_type=ogr.wkbMultiPolygon25D,
        options=["OVERWRITE=YES", "DESCRIPTION=Test polygonz layer"],
    )
    assert lyr is not None, "Create layer failed."

    create_fields(lyr)
    add_metadata(lyr)

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem("id", "")
    gdaltest.ngw_ds = None

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + ds_resource_id

    gdaltest.ngw_ds = gdal.OpenEx(
        url,
        gdal.OF_UPDATE,
        open_options=[
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )
    assert gdaltest.ngw_ds is not None, "Open datasource failed."

    for layer_name in [
        "test_pt_layer",
        "test_ln_layer",
        "test_pl_layer",
        "test_plz_layer",
    ]:
        lyr = gdaltest.ngw_ds.GetLayerByName(layer_name)
        assert lyr is not None, f"Get layer '{layer_name}' failed."

        md_item = lyr.GetMetadataItem("test_int.d", "NGW")
        assert (
            md_item == "777"
        ), f"Did not get expected layer metadata item. test_int.d is equal {md_item}, but should 777."

        md_item = lyr.GetMetadataItem("test_float.f", "NGW")
        assert float(md_item) == pytest.approx(
            777.555, abs=0.00001
        ), f"Did not get expected layer metadata item. test_float.f is equal {md_item}, but should 777.555."

        md_item = lyr.GetMetadataItem("test_string", "NGW")
        assert (
            md_item == "metadata test"
        ), f"Did not get expected layer metadata item. test_string is equal {md_item}, but should  'metadata test'."

        resource_type = lyr.GetMetadataItem("resource_type", "")
        assert (
            resource_type is not None
        ), "Did not get expected layer metadata item. Resourse type should be present."

        assert lyr.GetGeomType() != ogr.wkbUnknown and lyr.GetGeomType() != ogr.wkbNone

    # Test append field
    lyr = gdaltest.ngw_ds.CreateLayer(
        "test_append_layer",
        srs=sr,
        geom_type=ogr.wkbPoint,
        options=["OVERWRITE=YES", "DESCRIPTION=Test append point layer"],
    )
    assert lyr is not None, "Create layer failed."

    create_fields(lyr)
    assert lyr.SyncToDisk() == 0

    fld_defn = ogr.FieldDefn("STRFIELD_NEW", ogr.OFTString)
    fld_defn.SetAlternativeName("String field test new")
    lyr.CreateField(fld_defn)
    lyr.DeleteField(2)
    assert lyr.SyncToDisk() == 0


###############################################################################
# Check open single vector layer.


@pytest.mark.slow()
def test_ogr_ngw_6():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName("test_pt_layer")
    lyr_resource_id = lyr.GetMetadataItem("id", "")
    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + lyr_resource_id
    ds = gdal.OpenEx(
        url,
        open_options=[
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )
    assert (
        ds is not None and ds.GetLayerCount() == 1
    ), "Failed to open single vector layer."


###############################################################################
# Check insert, update and delete features.


@pytest.mark.slow()
def test_ogr_ngw_7():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName("test_pt_layer")

    f = ogr.Feature(lyr.GetLayerDefn())
    fill_fields(f)
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    ret = lyr.CreateFeature(f)
    assert (
        ret == 0 and f.GetFID() >= 0
    ), f"Create feature failed. Expected FID greater or equal 0, got {f.GetFID()}."

    fill_fields2(f)
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    ret = lyr.SetFeature(f)
    assert ret == 0, f"Failed to update feature #{f.GetFID()}."

    lyr.DeleteFeature(f.GetFID())

    # Expected fail to get feature
    gdal.ErrorReset()
    try:
        f = lyr.GetFeature(f.GetFID())
        assert f is None, f"Failed. Got deleted feature #{f.GetFID()}."
    except Exception:
        pass


###############################################################################
# Check insert, update features in batch mode.


@pytest.mark.slow()
def test_ogr_ngw_8():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem("id", "")
    gdaltest.ngw_ds = None

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + ds_resource_id
    gdaltest.ngw_ds = gdal.OpenEx(
        url,
        gdal.OF_UPDATE,
        open_options=[
            "BATCH_SIZE=2",
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )

    lyr = gdaltest.ngw_ds.GetLayerByName("test_pt_layer")
    f1 = ogr.Feature(lyr.GetLayerDefn())
    fill_fields(f1)
    f1.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    ret = lyr.CreateFeature(f1)
    assert ret == 0 and f1.GetFID() < 0

    f2 = ogr.Feature(lyr.GetLayerDefn())
    fill_fields2(f2)
    f2.SetGeometry(ogr.CreateGeometryFromWkt("POINT (2 3)"))
    ret = lyr.CreateFeature(f2)
    assert ret == 0 and f2.GetFID() < 0

    f3 = ogr.Feature(lyr.GetLayerDefn())
    fill_fields(f3)
    f3.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    ret = lyr.CreateFeature(f3)
    assert ret == 0

    ret = lyr.SyncToDisk()
    assert ret == 0

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    counter = 0
    while feat is not None:
        counter += 1
        assert (
            feat.GetFID() >= 0
        ), f"Expected FID greater or equal 0, got {feat.GetFID()}."

        feat = lyr.GetNextFeature()

    assert counter >= 3, f"Expected 3 or greater feature count, got {counter}."


###############################################################################
# Check paging while GetNextFeature.


@pytest.mark.slow()
def test_ogr_ngw_9():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem("id", "")
    gdaltest.ngw_ds = None

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + ds_resource_id
    gdaltest.ngw_ds = gdal.OpenEx(
        url,
        gdal.OF_UPDATE,
        open_options=[
            "PAGE_SIZE=2",
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )

    lyr = gdaltest.ngw_ds.GetLayerByName("test_pt_layer")

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    counter = 0
    while feat is not None:
        counter += 1
        assert (
            feat.GetFID() >= 0
        ), f"Expected FID greater or equal 0, got {feat.GetFID()}."

        feat = lyr.GetNextFeature()

    assert counter >= 3, f"Expected 3 or greater feature count, got {counter}."


###############################################################################
# Check native data.


@pytest.mark.slow()  # 6s
def test_ogr_ngw_10():
    if gdaltest.ngw_ds is None:
        pytest.skip()

    ds_resource_id = gdaltest.ngw_ds.GetMetadataItem("id", "")
    gdaltest.ngw_ds = None

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + ds_resource_id
    gdaltest.ngw_ds = gdal.OpenEx(
        url,
        gdal.OF_UPDATE,
        open_options=[
            "NATIVE_DATA=YES",
            "EXTENSIONS=description,attachment",
            f"TIMEOUT={NET_TIMEOUT}",
            f"MAX_RETRY={NET_MAX_RETRY}",
            f"RETRY_DELAY={NET_RETRY_DELAY}",
        ],
    )
    lyr = gdaltest.ngw_ds.GetLayerByName("test_pt_layer")
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    feature_id = feat.GetFID()
    native_data = feat.GetNativeData()
    assert (
        native_data is not None
    ), f"Feature #{feature_id} native data should not be empty"
    # {"description":null,"attachment":null}
    assert (
        feat.GetNativeMediaType() == "application/json"
    ), "Unsupported native media type"

    # Set description
    feat.SetNativeData('{"description":"Test feature description"}')
    ret = lyr.SetFeature(feat)
    assert ret == 0, f"Failed to update feature #{feature_id}."

    feat = lyr.GetFeature(feature_id)
    native_data = feat.GetNativeData()
    assert (
        native_data is not None and native_data.find("Test feature description") != -1
    ), f"Expected feature description text, got {native_data}"


###############################################################################
# Check ignored fields works ok


@pytest.mark.slow()
def test_ogr_ngw_11():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName("test_pt_layer")
    lyr.SetIgnoredFields(["STRFIELD"])

    feat = lyr.GetNextFeature()

    assert not feat.IsFieldSet("STRFIELD"), "got STRFIELD despite request to ignore it."

    assert feat.GetFieldAsInteger("DECFIELD") == 123, "missing or wrong DECFIELD"

    layerDefn = lyr.GetLayerDefn()
    fld = layerDefn.GetFieldDefn(0)  # STRFIELD

    assert (
        fld.GetName() == "STRFIELD"
    ), f"Expected field 'STRFIELD', got {fld.GetName()}"
    assert fld.IsIgnored(), "STRFIELD unexpectedly not marked as ignored."

    fld = layerDefn.GetFieldDefn(1)  # DECFIELD
    assert not fld.IsIgnored(), "DECFIELD unexpectedly marked as ignored."

    assert not layerDefn.IsGeometryIgnored(), "geometry unexpectedly ignored."

    assert not layerDefn.IsStyleIgnored(), "style unexpectedly ignored."

    feat = None
    lyr = None


###############################################################################
# Check attribute filter.


@pytest.mark.slow()
def test_ogr_ngw_12():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName("test_pt_layer")
    lyr.SetAttributeFilter("STRFIELD = 'русский'")
    fc = lyr.GetFeatureCount()
    assert fc == 1, f"Expected feature count is 1, got {fc}."

    lyr.SetAttributeFilter("STRFIELD = 'fo_o' AND DECFIELD = 321")
    fc = lyr.GetFeatureCount()
    assert fc == 0, f"Expected feature count is 0, got {fc}."

    lyr.SetAttributeFilter("NGW:fld_STRFIELD=fo_o&fld_DECFIELD=123")
    fc = lyr.GetFeatureCount()
    assert fc == 2, f"Expected feature count is 2, got {fc}."

    lyr.SetAttributeFilter("DECFIELD < 321")
    fc = lyr.GetFeatureCount()
    assert fc == 2, f"Expected feature count is 2, got {fc}."

    lyr.SetAttributeFilter("NGW:fld_REALFIELD__gt=1.5")
    fc = lyr.GetFeatureCount()
    assert fc == 1, f"Expected feature count is 1, got {fc}."

    lyr.SetAttributeFilter("STRFIELD ILIKE '%O_O'")
    fc = lyr.GetFeatureCount()
    assert fc == 2, f"Expected feature count is 2, got {fc}."


###############################################################################
# Check spatial filter.


@pytest.mark.slow()
def test_ogr_ngw_13():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName("test_pt_layer")

    # Reset any attribute filters
    lyr.SetAttributeFilter(None)

    # Check intersecting POINT(3 4)
    lyr.SetSpatialFilter(
        ogr.CreateGeometryFromWkt("POLYGON ((2.5 3.5,2.5 6,6 6,6 3.5,2.5 3.5))")
    )
    fc = lyr.GetFeatureCount()
    assert fc == 1, f"Expected feature count is 1, got {fc}."


###############################################################################
# Check ignore geometry.


@pytest.mark.slow()
def test_ogr_ngw_14():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    lyr = gdaltest.ngw_ds.GetLayerByName("test_pt_layer")

    # Reset any attribute filters
    lyr.SetAttributeFilter(None)
    lyr.SetSpatialFilter(None)

    fd = lyr.GetLayerDefn()
    fd.SetGeometryIgnored(1)

    assert fd.IsGeometryIgnored(), "geometry unexpectedly not ignored."

    feat = lyr.GetNextFeature()

    assert feat.GetGeometryRef() is None, "Unexpectedly got a geometry on feature 2."

    feat = None


###############################################################################
# Check ExecuteSQL.


@pytest.mark.slow()  # 10s
def test_ogr_ngw_15():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    gdaltest.ngw_ds.ExecuteSQL("DELLAYER:test_ln_layer")
    lyr = gdaltest.ngw_ds.GetLayerByName("test_ln_layer")
    assert lyr is None, "Expected fail to get layer test_ln_layer."

    lyr = gdaltest.ngw_ds.GetLayerByName("test_pl_layer")

    f = ogr.Feature(lyr.GetLayerDefn())
    fill_fields(f)
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 0,0 0))"))
    ret = lyr.CreateFeature(f)
    assert ret == 0, "Failed to create feature in test_pl_layer."

    fill_fields2(f)
    ret = lyr.CreateFeature(f)
    assert ret == 0, "Failed to create feature in test_pl_layer."

    fill_fields(f)
    ret = lyr.CreateFeature(f)
    assert ret == 0, "Failed to create feature in test_pl_layer."

    assert (
        lyr.GetFeatureCount() == 3
    ), f"Expected feature count is 3, got {lyr.GetFeatureCount()}."

    gdaltest.ngw_ds.ExecuteSQL("DELETE FROM test_pl_layer WHERE \"STRFIELD\" = 'fo_o'")
    assert (
        lyr.GetFeatureCount() == 1
    ), f"Expected feature count is 1, got {lyr.GetFeatureCount()}."

    gdaltest.ngw_ds.ExecuteSQL("DELETE FROM test_pl_layer")
    assert (
        lyr.GetFeatureCount() == 0
    ), f"Expected feature count is 0, got {lyr.GetFeatureCount()}."

    gdaltest.ngw_ds.ExecuteSQL("ALTER TABLE test_pl_layer RENAME TO test_pl_layer777")
    lyr = gdaltest.ngw_ds.GetLayerByName("test_pl_layer777")
    assert lyr is not None, "Get layer test_pl_layer777 failed."

    # Create 2 new features

    f = ogr.Feature(lyr.GetLayerDefn())
    fill_fields(f)
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 0,0 0))"))
    ret = lyr.CreateFeature(f)
    assert ret == 0, "Failed to create feature in test_pl_layer777."

    f = ogr.Feature(lyr.GetLayerDefn())
    fill_fields2(f)
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((1 1,1 2,2 1,1 1))"))
    ret = lyr.CreateFeature(f)
    assert ret == 0, "Failed to create feature in test_pl_layer777."

    lyr = gdaltest.ngw_ds.ExecuteSQL(
        "SELECT STRFIELD,DECFIELD FROM test_pl_layer777 WHERE STRFIELD = 'fo_o'"
    )
    assert (
        lyr is not None
    ), 'ExecuteSQL: SELECT STRFIELD,DECFIELD FROM test_pl_layer777 WHERE STRFIELD = "fo_o"; failed.'
    assert (
        lyr.GetFeatureCount() == 2
    ), f"Expected feature count is 2, got {lyr.GetFeatureCount()}."

    gdaltest.ngw_ds.ReleaseResultSet(lyr)


###############################################################################
# Test field domains


@pytest.mark.slow()
def test_ogr_ngw_16():
    if gdaltest.ngw_ds is None:
        pytest.skip()

    assert gdaltest.ngw_ds.TestCapability(ogr.ODsCAddFieldDomain)

    assert gdaltest.ngw_ds.GetFieldDomain("does_not_exist") is None

    base_name = f"enum_domain_{str(int(time.time()))}_{str(random.randint(10, 99))}"

    assert gdaltest.ngw_ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            base_name,
            "test domain",
            ogr.OFTInteger64,
            ogr.OFSTNone,
            {1: "one", "2": None},
        )
    )
    assert gdaltest.ngw_ds.GetFieldDomain(base_name) is not None

    assert gdaltest.ngw_ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            f"{base_name}_guess_int_single",
            "my desc",
            ogr.OFTInteger,
            ogr.OFSTNone,
            {1: "one"},
        )
    )
    assert gdaltest.ngw_ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            f"{base_name}_guess_int",
            "",
            ogr.OFTInteger,
            ogr.OFSTNone,
            {1: "one", 2: "two"},
        )
    )
    assert gdaltest.ngw_ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            f"{base_name}_guess_int64_single_1",
            "",
            ogr.OFTInteger64,
            ogr.OFSTNone,
            {1234567890123: "1234567890123"},
        )
    )
    assert gdaltest.ngw_ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            f"{base_name}_guess_int64_single_2",
            "",
            ogr.OFTInteger64,
            ogr.OFSTNone,
            {-1234567890123: "-1234567890123"},
        )
    )
    assert gdaltest.ngw_ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            f"{base_name}_guess_int64",
            "",
            ogr.OFTInteger64,
            ogr.OFSTNone,
            {1: "one", 1234567890123: "1234567890123", 3: "three"},
        )
    )
    assert gdaltest.ngw_ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            f"{base_name}_guess_string_single",
            "",
            ogr.OFTString,
            ogr.OFSTNone,
            {"three": "three"},
        )
    )
    assert gdaltest.ngw_ds.AddFieldDomain(
        ogr.CreateCodedFieldDomain(
            f"{base_name}_guess_string",
            "",
            ogr.OFTString,
            ogr.OFSTNone,
            {1: "one", 1.5: "one dot five", "three": "three", 4: "four"},
        )
    )

    assert {
        base_name,
        f"{base_name}_guess_int",
        f"{base_name}_guess_int64",
        f"{base_name}_guess_int64_single_1",
        f"{base_name}_guess_int64_single_2",
        f"{base_name}_guess_int_single",
        f"{base_name}_guess_string",
        f"{base_name}_guess_string_single",
    }.issubset(set(gdaltest.ngw_ds.GetFieldDomainNames()))

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3857)
    lyr = gdaltest.ngw_ds.CreateLayer(
        "test_pt_layer_dom",
        srs=sr,
        geom_type=ogr.wkbPoint,
        options=[
            "OVERWRITE=FALSE",
            "DESCRIPTION=Test point layer with coded field domains",
        ],
    )
    assert lyr is not None, "Create layer failed."

    fld_defn = ogr.FieldDefn("with_enum_domain", ogr.OFTInteger64)
    fld_defn.SetDomainName(f"{base_name} (bigint)")
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn("without_domain_initially", ogr.OFTInteger)
    lyr.CreateField(fld_defn)

    # If all keys can be transform to numbers 3 domains created for string, int
    # and int64 types
    domain1 = gdaltest.ngw_ds.GetFieldDomain(base_name)
    assert domain1 is not None
    assert domain1.GetName() == base_name
    assert domain1.GetDescription() == "test domain"
    assert domain1.GetDomainType() == ogr.OFDT_CODED
    assert domain1.GetFieldType() == ogr.OFTString
    assert domain1.GetEnumeration() == {
        "1": "one",
        "2": "",
    }, f'Expected \'{{"1": "one", "2": ""}}\', got {domain1.GetEnumeration()}'

    domain2 = gdaltest.ngw_ds.GetFieldDomain(f"{base_name} (number)")
    assert domain2 is not None
    assert domain2.GetName() == f"{base_name} (number)"
    assert domain2.GetDescription() == "test domain"
    assert domain2.GetDomainType() == ogr.OFDT_CODED
    assert domain2.GetFieldType() == ogr.OFTInteger
    assert domain2.GetEnumeration() == {
        "1": "one",
        "2": "",
    }, f'Expected \'{{"1": "one", "2": ""}}\', got {domain1.GetEnumeration()}'

    domain3 = gdaltest.ngw_ds.GetFieldDomain(f"{base_name} (bigint)")
    assert domain3 is not None
    assert domain3.GetName() == f"{base_name} (bigint)"
    assert domain3.GetDescription() == "test domain"
    assert domain3.GetDomainType() == ogr.OFDT_CODED
    assert domain3.GetFieldType() == ogr.OFTInteger64
    assert domain3.GetEnumeration() == {
        "1": "one",
        "2": "",
    }, f'Expected \'{{"1": "one", "2": ""}}\', got {domain1.GetEnumeration()}'

    domain = gdaltest.ngw_ds.GetFieldDomain(f"{base_name}_guess_int_single (number)")
    assert domain.GetDescription() == "my desc"
    assert domain.GetFieldType() == ogr.OFTInteger

    domain = gdaltest.ngw_ds.GetFieldDomain(f"{base_name}_guess_int (number)")
    assert domain.GetFieldType() == ogr.OFTInteger

    domain = gdaltest.ngw_ds.GetFieldDomain(
        f"{base_name}_guess_int64_single_1 (bigint)"
    )
    assert domain.GetFieldType() == ogr.OFTInteger64

    domain = gdaltest.ngw_ds.GetFieldDomain(
        f"{base_name}_guess_int64_single_2 (bigint)"
    )
    assert domain.GetFieldType() == ogr.OFTInteger64

    domain = gdaltest.ngw_ds.GetFieldDomain(f"{base_name}_guess_int64 (bigint)")
    assert domain.GetFieldType() == ogr.OFTInteger64

    domain = gdaltest.ngw_ds.GetFieldDomain(f"{base_name}_guess_string_single")
    assert domain.GetFieldType() == ogr.OFTString

    domain = gdaltest.ngw_ds.GetFieldDomain(f"{base_name}_guess_string")
    assert domain.GetFieldType() == ogr.OFTString

    lyr_defn = lyr.GetLayerDefn()
    # Unset domain name
    idx = lyr_defn.GetFieldIndex("with_enum_domain")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    fld_defn.SetDomainName("")
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    # Change domain name
    idx = lyr_defn.GetFieldIndex("with_enum_domain")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    fld_defn.SetDomainName(f"{base_name}_guess_int64 (bigint)")
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    # Don't change anything
    idx = lyr_defn.GetFieldIndex("with_enum_domain")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    # Set domain name
    idx = lyr_defn.GetFieldIndex("without_domain_initially")
    fld_defn = lyr_defn.GetFieldDefn(idx)
    fld_defn.SetDomainName(f"{base_name}_guess_int (number)")
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert (
        fld_defn.GetDomainName() == f"{base_name}_guess_int (number)"
    ), f"Got {fld_defn.GetDomainName()} but expected '{base_name}_guess_int (number)'"


###############################################################################
#  Run test_ogrsf


@pytest.mark.slow()  # 460 s
def test_ogr_ngw_test_ogrsf():
    # FIXME: depends on previous test
    if gdaltest.ngw_ds is None:
        pytest.skip()

    gdaltest.skip_on_travis()

    url = "NGW:" + gdaltest.ngw_test_server + "/resource/" + gdaltest.group_id

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + " " + url)
    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " "
        + url
        + f" -oo PAGE_SIZE=100 -oo TIMEOUT={NET_TIMEOUT}"
        + f" -oo MAX_RETRY={NET_MAX_RETRY} -oo RETRY_DELAY={NET_RETRY_DELAY}"
    )
    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " "
        + url
        + f" -oo BATCH_SIZE=5 -oo TIMEOUT={NET_TIMEOUT}"
        + f" -oo MAX_RETRY={NET_MAX_RETRY} -oo RETRY_DELAY={NET_RETRY_DELAY}"
    )
    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " "
        + url
        + f" -oo BATCH_SIZE=5 -oo PAGE_SIZE=100 -oo TIMEOUT={NET_TIMEOUT}"
        + f" -oo MAX_RETRY={NET_MAX_RETRY} -oo RETRY_DELAY={NET_RETRY_DELAY}"
    )
    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1
