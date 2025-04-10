#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Style testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import pytest

from osgeo import gdal, ogr

###############################################################################
#
#


def test_ogr_style_styletable():

    style_table = ogr.StyleTable()
    style_table.AddStyle(
        "style1_normal", 'SYMBOL(id:"http://style1_normal",c:#67452301)'
    )
    with pytest.raises(Exception):
        style_table.SaveStyleTable("/nonexistingdir/nonexistingfile")
    assert style_table.SaveStyleTable("/vsimem/out.txt") == 1
    style_table = None

    style_table = ogr.StyleTable()
    with pytest.raises(Exception):
        style_table.LoadStyleTable("/nonexistent")
    assert style_table.LoadStyleTable("/vsimem/out.txt") == 1

    gdal.Unlink("/vsimem/out.txt")

    with gdal.quiet_errors():
        ret = style_table.Find("non_existing_style")
    assert ret is None

    assert (
        style_table.Find("style1_normal")
        == 'SYMBOL(id:"http://style1_normal",c:#67452301)'
    )

    style = style_table.GetNextStyle()
    assert style == 'SYMBOL(id:"http://style1_normal",c:#67452301)'
    style_name = style_table.GetLastStyleName()
    assert style_name == "style1_normal"

    style = style_table.GetNextStyle()
    assert style is None

    style_table.ResetStyleStringReading()
    style = style_table.GetNextStyle()
    assert style is not None

    # GetStyleTable()/SetStyleTable() on data source
    ds = ogr.GetDriverByName("MEM").CreateDataSource("")
    assert ds.GetStyleTable() is None
    ds.SetStyleTable(None)
    assert ds.GetStyleTable() is None
    ds.SetStyleTable(style_table)
    style_table2 = ds.GetStyleTable()
    style = style_table2.GetNextStyle()
    assert style == 'SYMBOL(id:"http://style1_normal",c:#67452301)'

    # GetStyleTable()/SetStyleTable() on layer
    lyr = ds.CreateLayer("foo")
    assert lyr.GetStyleTable() is None
    lyr.SetStyleTable(None)
    assert lyr.GetStyleTable() is None
    lyr.SetStyleTable(style_table)
    style_table2 = lyr.GetStyleTable()
    style = style_table2.GetNextStyle()
    assert style == 'SYMBOL(id:"http://style1_normal",c:#67452301)'

    ds = None
