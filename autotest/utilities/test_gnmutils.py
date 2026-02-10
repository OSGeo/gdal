#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic GNMGdalNetwork class functionality.
# Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
#           Dmitry Baryshnikov, polimax@mail.ru
#
###############################################################################
# Copyright (c) 2014, Mikhail Gusev
# Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest
import test_cli_utilities

pytestmark = [
    pytest.mark.skipif(
        test_cli_utilities.get_gnmmanage_path() is None,
        reason="gnmmanage not available",
    ),
    pytest.mark.skipif(
        test_cli_utilities.get_gnmanalyse_path() is None,
        reason="gnmanalyse not available",
    ),
    pytest.mark.random_order(disabled=True),
    pytest.mark.xdist_group("test_gnmutils"),
]


@pytest.fixture()
def gnmmanage_path():
    return test_cli_utilities.get_gnmmanage_path()


@pytest.fixture()
def gnmanalyse_path():
    return test_cli_utilities.get_gnmanalyse_path()


@pytest.fixture(scope="module")
def test_gnm_dir(tmp_path_factory):
    return tmp_path_factory.mktemp("test_gnmutiles") / "test_gnm"


###############################################################################
# Test create
# gnmmanage create -f GNMFile -t_srs EPSG:4326 -dsco net_name=test_gnm -dsco net_description="Test file based GNM" /home/bishop/tmp/ --config CPL_DEBUG ON


def test_gnmmanage_1(gnmmanage_path, test_gnm_dir):

    _, err = gdaltest.runexternal_out_and_err(
        gnmmanage_path
        + f' create -f GNMFile -t_srs EPSG:4326 -dsco net_name=test_gnm -dsco net_description="Test file based GNM" {test_gnm_dir.parent}'
    )
    assert err is None or err == "", "got error/warning"

    assert os.path.exists(test_gnm_dir)


###############################################################################
# Test import
# gnmmanage import /home/bishop/tmp/data/pipes.shp /home/bishop/tmp/test_gnm --config CPL_DEBUG ON
# gnmmanage import /home/bishop/tmp/data/wells.shp /home/bishop/tmp/test_gnm --config CPL_DEBUG ON


def test_gnmmanage_2(gnmmanage_path, test_gnm_dir):

    _, err = gdaltest.runexternal_out_and_err(
        f"{gnmmanage_path} import ../gnm/data/pipes.shp {test_gnm_dir}"
    )
    assert err is None or err == "", "got error/warning"

    _, err = gdaltest.runexternal_out_and_err(
        f"{gnmmanage_path} import ../gnm/data/wells.shp {test_gnm_dir}"
    )
    assert err is None or err == "", "got error/warning"


###############################################################################
# Test info
# gnmmanage info /home/bishop/tmp/test_gnm


def test_gnmmanage_3(gnmmanage_path, test_gnm_dir):

    ret = gdaltest.runexternal(f"{gnmmanage_path} info {test_gnm_dir}")

    assert ret.find("Network version: 1.0.") != -1
    assert ret.find("Network name: test_gnm.") != -1
    assert ret.find("Network description") != -1


###############################################################################
# Test autoconect
# gnmmanage autoconnect 0.000001 /home/bishop/tmp/test_gnm --config CPL_DEBUG ON


def test_gnmmanage_4(gnmmanage_path, test_gnm_dir):

    ret = gdaltest.runexternal(f"{gnmmanage_path} autoconnect 0.000001 {test_gnm_dir}")
    assert ret.find("success") != -1


###############################################################################
# Test dijkstra
# gnmanalyse dijkstra 61 50 -alo "fetch_vertex=OFF" -ds /home/bishop/tmp/di.shp -lco "SHPT=ARC" /home/bishop/tmp/test_gnm --config CPL_DEBUG ON


def test_gnmanalyse_1(gnmanalyse_path, test_gnm_dir):

    ret = gdaltest.runexternal(f"{gnmanalyse_path} dijkstra 61 50 {test_gnm_dir}")
    assert ret.find("Feature Count: 19") != -1


###############################################################################
# Test kpaths
# gnmanalyse kpaths 61 50 3 -alo "fetch_vertex=OFF" -ds /home/bishop/tmp/kp.shp -lco "SHPT=ARC" /home/bishop/tmp/test_gnm --config CPL_DEBUG ON


def test_gnmanalyse_2(gnmanalyse_path, test_gnm_dir):

    ret = gdaltest.runexternal(f"{gnmanalyse_path} kpaths 61 50 3 {test_gnm_dir}")
    assert ret.find("Feature Count: 61") != -1


###############################################################################
# Test cleanup


def test_gnm_cleanup(gnmmanage_path, test_gnm_dir):

    _, err = gdaltest.runexternal_out_and_err(f"{gnmmanage_path} delete {test_gnm_dir}")
    assert err is None or err == "", "got error/warning"

    assert not os.path.exists(test_gnm_dir)
