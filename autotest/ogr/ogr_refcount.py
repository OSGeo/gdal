#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test "shared" open, and various refcount based stuff.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
import pytest

from osgeo import gdal, ogr


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Open two datasets in shared mode.


def test_ogr_refcount_1():
    # if ogr.GetOpenDSCount() != 0:
    #    gdaltest.post_reason( 'Initial Open DS count is not zero!' )
    #    return 'failed'

    ds_1 = ogr.OpenShared("data/idlink.dbf")
    ds_2 = ogr.OpenShared("data/poly.shp")

    # if ogr.GetOpenDSCount() != 2:
    #    gdaltest.post_reason( 'Open DS count not 2 after shared opens.' )
    #    return 'failed'

    assert ds_1.GetRefCount() == 1
    assert ds_2.GetRefCount() == 1


###############################################################################
# Verify that reopening one of the datasets returns the existing shared handle.


def test_ogr_refcount_2():

    ds_1 = ogr.OpenShared("data/idlink.dbf")
    ds_3 = ogr.OpenShared("data/idlink.dbf")

    assert ds_1 is not None
    assert ds_3 is not None

    # if ogr.GetOpenDSCount() != 2:
    #    gdaltest.post_reason( 'Open DS count not 2 after third open.' )
    #    return 'failed'

    assert ds_3.GetRefCount() == 2, "Refcount not 2 after reopened."


###############################################################################
# Verify that releasing the datasources has the expected behaviour.


def test_ogr_refcount_3():

    ds_1 = ogr.OpenShared("data/idlink.dbf")
    ds_3 = ogr.OpenShared("data/idlink.dbf")

    assert ds_1 is not None
    assert ds_3 is not None

    ds_3.Release()

    assert ds_1.GetRefCount() == 1


###############################################################################
# Verify that we can walk the open datasource list.


def test_ogr_refcount_4():

    ds_1 = ogr.OpenShared("data/idlink.dbf")
    ds_2 = ogr.OpenShared("data/poly.shp")

    assert ds_1 is not None
    assert ds_2 is not None

    with gdal.quiet_errors():
        ogr.GetOpenDS(0)
