#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR VFK driver functionality.
# Author:   Martin Landa <landa.martin gmail.com>
#
###############################################################################
# Copyright (c) 2009-2019 Martin Landa <landa.martin gmail.com>
# Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pathlib
import shutil

import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("VFK")


@pytest.fixture(scope="module", autouse=True)
def setup_and_cleanup():

    with gdal.config_option("OGR_VFK_DB_OVERWRITE", "YES"):
        yield


@pytest.fixture()
def vfk_ds(tmp_path):

    shutil.copy("data/vfk/bylany.vfk", tmp_path)

    ds = ogr.Open(tmp_path / "bylany.vfk")

    return ds


###############################################################################
# Open file, check number of layers, get first layer,
# check number of fields and features


def test_ogr_vfk_1(vfk_ds):

    assert vfk_ds is not None

    assert vfk_ds.GetLayerCount() == 61, "expected exactly 61 layers!"

    vfk_layer_par = vfk_ds.GetLayer(0)

    assert vfk_layer_par is not None, "cannot get first layer"

    assert vfk_layer_par.GetName() == "PAR", 'did not get expected layer name "PAR"'

    defn = vfk_layer_par.GetLayerDefn()
    assert defn.GetFieldCount() == 28, (
        "did not get expected number of fields, got %d" % defn.GetFieldCount()
    )

    fc = vfk_layer_par.GetFeatureCount()
    assert fc == 1, "did not get expected feature count, got %d" % fc


###############################################################################
# Read the first feature from layer 'PAR', check envelope


@pytest.mark.require_geos
def test_ogr_vfk_2(vfk_ds):

    vfk_layer_par = vfk_ds.GetLayer(0)

    vfk_layer_par.ResetReading()

    feat = vfk_layer_par.GetNextFeature()

    assert feat.GetFID() == 1, "did not get expected fid for feature 1"

    geom = feat.GetGeometryRef()
    assert (
        geom.GetGeometryType() == ogr.wkbPolygon
    ), "did not get expected geometry type."

    envelope = geom.GetEnvelope()
    area = (envelope[1] - envelope[0]) * (envelope[3] - envelope[2])
    exp_area = 2010.5

    assert area >= exp_area - 0.5 and area <= exp_area + 0.5, (
        "envelope area not as expected, got %g." % area
    )


###############################################################################
# Read features from layer 'SOBR', test attribute query


def test_ogr_vfk_3(vfk_ds):

    vfk_layer_sobr = vfk_ds.GetLayer(43)

    assert vfk_layer_sobr.GetName() == "SOBR", 'did not get expected layer name "SOBR"'

    vfk_layer_sobr.SetAttributeFilter("CISLO_BODU = '55'")

    vfk_layer_sobr.ResetReading()

    feat = vfk_layer_sobr.GetNextFeature()
    count = 0
    while feat:
        feat = vfk_layer_sobr.GetNextFeature()
        count += 1

    assert count == 1, "did not get expected number of features, got %d" % count


###############################################################################
# Read features from layer 'SBP', test random access, check length


def test_ogr_vfk_4(vfk_ds):

    vfk_layer_sbp = vfk_ds.GetLayerByName("SBP")

    assert vfk_layer_sbp, 'did not get expected layer name "SBP"'

    feat = vfk_layer_sbp.GetFeature(5)
    length = int(feat.geometry().Length())

    assert length == 10, "did not get expected length, got %d" % length


###############################################################################
# Read features from layer 'HP', check geometry type


def test_ogr_vfk_5(vfk_ds):

    vfk_layer_hp = vfk_ds.GetLayerByName("HP")

    assert vfk_layer_hp != "HP", 'did not get expected layer name "HP"'

    geom_type = vfk_layer_hp.GetGeomType()

    assert geom_type == ogr.wkbLineString, (
        "did not get expected geometry type, got %d" % geom_type
    )


###############################################################################
# Re-Open file (test .db persistence)


def test_ogr_vfk_6(vfk_ds):

    dsn = vfk_ds.GetDescription()
    vfk_ds.Close()

    vfk_ds = ogr.Open(dsn)

    assert vfk_ds is not None

    assert vfk_ds.GetLayerCount() == 61, "expected exactly 61 layers!"

    vfk_layer_par = vfk_ds.GetLayer(0)

    assert vfk_layer_par is not None, "cannot get first layer"

    assert vfk_layer_par.GetName() == "PAR", 'did not get expected layer name "PAR"'

    defn = vfk_layer_par.GetLayerDefn()
    assert defn.GetFieldCount() == 28, (
        "did not get expected number of fields, got %d" % defn.GetFieldCount()
    )

    fc = vfk_layer_par.GetFeatureCount()
    assert fc == 1, "did not get expected feature count, got %d" % fc


###############################################################################
# Read PAR layer, check data types (Integer64 new in GDAL 2.2)


def test_ogr_vfk_7(vfk_ds):

    vfk_layer_par = vfk_ds.GetLayer(0)

    defn = vfk_layer_par.GetLayerDefn()

    for idx, name, ctype in (
        (0, "ID", ogr.OFTInteger64),
        (1, "STAV_DAT", ogr.OFTInteger),
        (2, "DATUM_VZNIKU", ogr.OFTString),
        (22, "CENA_NEMOVITOSTI", ogr.OFTReal),
    ):
        col = defn.GetFieldDefn(idx)
        assert (
            col.GetName() == name and col.GetType() == ctype
        ), "PAR: '{}' column name/type mismatch".format(name)


###############################################################################
# Open DB file as datasource (new in GDAL 2.2)


def test_ogr_vfk_8(vfk_ds):

    dsn = pathlib.Path(vfk_ds.GetDescription())
    vfk_ds.Close()

    # open by SQLite driver first
    vfk_ds_db = ogr.Open(dsn.with_suffix(".db"))
    assert vfk_ds_db.GetDriver().GetName() == "SQLite"
    count1 = vfk_ds_db.GetLayerCount()
    vfk_ds_db = None

    # then open by VFK driver
    with gdal.config_option("OGR_VFK_DB_READ", "YES"):
        vfk_ds_db = ogr.Open(dsn.with_suffix(".db"))
        assert vfk_ds_db.GetDriver().GetName() == "VFK"
        count2 = vfk_ds_db.GetLayerCount()
        vfk_ds_db = None

    assert (
        count1 == count2
    ), "layer count differs when opening DB by SQLite and VFK drivers"


###############################################################################
# Open datasource with SUPPRESS_GEOMETRY open option (new in GDAL 2.3)


def test_ogr_vfk_9(vfk_ds):

    # open with suppressing geometry
    dsn = vfk_ds.GetDescription()
    vfk_ds.Close()

    vfk_ds = gdal.OpenEx(dsn, open_options=["SUPPRESS_GEOMETRY=YES"])

    vfk_layer_par = vfk_ds.GetLayerByName("PAR")

    assert vfk_layer_par != "PAR", 'did not get expected layer name "PAR"'

    geom_type = vfk_layer_par.GetGeomType()
    vfk_layer_par = None
    vfk_ds = None

    assert geom_type == ogr.wkbNone, (
        "did not get expected geometry type, got %d" % geom_type
    )


###############################################################################
# Open datasource with FILE_FIELD open option (new in GDAL 2.4)


def test_ogr_vfk_10(vfk_ds):

    # open with suppressing geometry
    dsn = vfk_ds.GetDescription()
    vfk_ds.Close()

    vfk_ds = gdal.OpenEx(dsn, open_options=["FILE_FIELD=YES"])

    vfk_layer_par = vfk_ds.GetLayerByName("PAR")

    assert vfk_layer_par != "PAR", 'did not get expected layer name "PAR"'

    vfk_layer_par.ResetReading()
    feat = vfk_layer_par.GetNextFeature()
    file_field = feat.GetField("VFK_FILENAME")
    vfk_layer_par = None
    vfk_ds = None

    assert file_field == "bylany.vfk", "did not get expected file field value"


###############################################################################
# Read PAR layer, check sequential feature access consistency


def test_ogr_vfk_11(vfk_ds):

    vfk_layer_par = vfk_ds.GetLayer(0)

    def count_features():
        vfk_layer_par.ResetReading()
        count = 0
        while True:
            feat = vfk_layer_par.GetNextFeature()
            if not feat:
                break
            count += 1

        return count

    count = vfk_layer_par.GetFeatureCount()
    for i in range(2):  # perform check twice, mix with random access
        if count != count_features():
            feat = vfk_layer_par.GetFeature(i)
            feat.DumpReadable()
            pytest.fail("did not get expected number of features")


###############################################################################
# Read SBP layer, check curved geometry


def test_ogr_vfk_12(vfk_ds):

    vfk_layer_sbp = vfk_ds.GetLayerByName("SBP")

    vfk_layer_sbp.SetAttributeFilter("PARAMETRY_SPOJENI = '16'")
    feat = vfk_layer_sbp.GetNextFeature()
    geom = feat.GetGeometryRef()

    assert (
        geom.GetGeometryType() == ogr.wkbLineString
    ), "did not get expected geometry type."

    assert geom.GetPointCount() == 92, "did not get expected number of points."


###############################################################################
# Read the first feature from layer 'BUD', check geometry type


@pytest.mark.require_geos
def test_ogr_vfk_14(vfk_ds):

    vfk_layer_bud = vfk_ds.GetLayerByName("BUD")

    vfk_layer_bud.ResetReading()

    feat = vfk_layer_bud.GetNextFeature()

    geom = feat.GetGeometryRef()
    assert (
        geom.GetGeometryType() == ogr.wkbMultiPolygon
    ), "did not get expected geometry type."
