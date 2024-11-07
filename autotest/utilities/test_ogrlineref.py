#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ogrlineref testing
# Author:   Dmitry Baryshnikov. polimax@mail.ru
#
###############################################################################
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import sys

import pytest

sys.path.append("../ogr")

import gdaltest
import test_cli_utilities

from osgeo import ogr

pytestmark = [
    pytest.mark.skipif(
        test_cli_utilities.get_ogrlineref_path() is None,
        reason="ogrlineref not available",
    ),
    pytest.mark.require_geos,
]


@pytest.fixture(scope="module")
def ogrlineref_path():
    return test_cli_utilities.get_ogrlineref_path()


@pytest.fixture(scope="module")
def parts_shp(ogrlineref_path, tmp_path_factory):

    parts_shp_fname = str(tmp_path_factory.mktemp("tmp") / "parts.shp")

    _, err = gdaltest.runexternal_out_and_err(
        ogrlineref_path
        + f" -create -l data/path.shp -p data/mstones.shp -pm pos -o {parts_shp_fname} -s 1000"
    )
    assert err is None or err == "", 'got error/warning: "%s"' % err

    with ogr.Open(parts_shp_fname) as ds:
        assert ds.GetLayer(0).GetFeatureCount() == 9

    yield parts_shp_fname


###############################################################################
# get_pos test


def test_ogrlineref_2(ogrlineref_path, parts_shp):

    ret = gdaltest.runexternal(
        ogrlineref_path + f" -get_pos -r {parts_shp} -x -1.4345 -y 51.9497 -quiet"
    ).strip()

    expected = "15977.724709"
    assert ret == expected, '"%s" != %s' % (ret.strip(), expected)


###############################################################################
# get_coord test


def test_ogrlineref_3(ogrlineref_path, parts_shp):

    ret = gdaltest.runexternal(
        ogrlineref_path + f" -get_coord -r {parts_shp} -m 15977.724709 -quiet"
    ).strip()

    expected = "-1.435097,51.950080,0.000000"
    assert ret == expected, "%s != %s" % (ret.strip(), expected)


###############################################################################
# get_subline test


def test_ogrlineref_4(ogrlineref_path, parts_shp, tmp_path):

    output_shp = str(tmp_path / "subline.shp")

    gdaltest.runexternal(
        f"{ogrlineref_path} -get_subline -r {parts_shp} -mb 13300 -me 17400 -o {output_shp}"
    )

    ds = ogr.Open(output_shp)
    assert ds is not None, "ds is None"

    feature_count = ds.GetLayer(0).GetFeatureCount()
    assert feature_count == 1, "feature count %d != 1" % feature_count
    ds = None


###############################################################################
# test kml


@pytest.mark.require_driver("KML")
def test_ogrlineref_5(ogrlineref_path, tmp_path):

    parts_kml = str(tmp_path / "parts.kml")

    gdaltest.runexternal_out_and_err(
        f'{ogrlineref_path} -create -f "KML" -l data/path.shp -p data/mstones.shp -pm pos -o {parts_kml} -s 222'
    )

    assert os.path.exists(parts_kml)
