#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id: ogr_sxf.py 26513 2013-10-02 11:59:50Z bishop $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR SXF driver functionality.
# Author:   Dmitry Baryshnikov <polimax@mail.ru>
#
###############################################################################
# Copyright (c) 2013, NextGIS
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

import shutil

import gdaltest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("SXF")

###############################################################################
# Open SXF datasource.


@pytest.fixture()
def test_ogr_sxf_1():

    with gdal.quiet_errors():
        # Expect Warning 0 and Warning 6.
        ds = ogr.Open("data/sxf/100_test.sxf")

    assert ds is not None


###############################################################################
# Run test_ogrsf


def test_ogr_sxf_2():

    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " data/sxf/100_test.sxf"
    )

    assert ret.find("INFO") != -1 and ret.find("ERROR") == -1


###############################################################################
# Open SXF datasource with custom RSC file.


def test_ogr_sxf_3():

    lyr_names = ["SYSTEM", "Not_Classified"]
    sxf_name = "tmp/test_ogr_sxf_3.sxf"
    rsc_name = "tmp/test_ogr_sxf_3.rsc"
    fake_rsc = open(rsc_name, "w")
    fake_rsc.close()
    shutil.copy("data/sxf/100_test.sxf", sxf_name)
    sxf_ds = gdal.OpenEx(
        sxf_name, gdal.OF_VECTOR, open_options=["SXF_RSC_FILENAME=" + rsc_name]
    )

    assert sxf_ds is not None

    for layer_n in range(sxf_ds.GetLayerCount()):
        lyr = sxf_ds.GetLayer(layer_n)
        assert lyr_names[layer_n] == lyr.GetName()


###############################################################################
# Open SXF datasource with layers fullname.


def test_ogr_sxf_4():

    lyr_names = [
        "СИСТЕМНЫЙ",
        "ВОДНЫЕ ОБЪЕКТЫ",
        "НАСЕЛЕННЫЕ ПУНКТЫ",
        "ИНФРАСТРУКТУРА",
        "ЗЕМЛЕПОЛЬЗОВАНИЕ",
        "РЕЛЬЕФ СУШИ",
        "ГИДРОГРАФИЯ (РЕЛЬЕФ)",
        "МАТЕМАТИЧЕСКАЯ ОСНОВА",
        "Not_Classified",
    ]
    sxf_name = "data/sxf/100_test.sxf"
    sxf_ds = gdal.OpenEx(
        sxf_name, gdal.OF_VECTOR, open_options=["SXF_LAYER_FULLNAME=YES"]
    )

    assert sxf_ds is not None
    assert sxf_ds.GetLayerCount() == len(lyr_names)

    actual_layer_names = []
    for layer_n in range(sxf_ds.GetLayerCount()):
        lyr = sxf_ds.GetLayer(layer_n)
        if lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1:
            pytest.skip("skipping test: recode is not possible")
        actual_layer_names.append(lyr.GetName())

    assert actual_layer_names == lyr_names
