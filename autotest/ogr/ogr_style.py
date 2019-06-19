#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Style testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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



from osgeo import ogr
from osgeo import gdal

###############################################################################
#
#


def test_ogr_style_styletable():

    style_table = ogr.StyleTable()
    style_table.AddStyle("style1_normal", 'SYMBOL(id:"http://style1_normal",c:#67452301)')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = style_table.SaveStyleTable('/nonexistingdir/nonexistingfile')
    gdal.PopErrorHandler()
    assert ret == 0
    assert style_table.SaveStyleTable("/vsimem/out.txt") == 1
    style_table = None

    style_table = ogr.StyleTable()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = style_table.LoadStyleTable('/nonexistent')
    gdal.PopErrorHandler()
    assert ret == 0
    assert style_table.LoadStyleTable('/vsimem/out.txt') == 1

    gdal.Unlink('/vsimem/out.txt')

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = style_table.Find("non_existing_style")
    gdal.PopErrorHandler()
    assert ret is None

    assert style_table.Find("style1_normal") == 'SYMBOL(id:"http://style1_normal",c:#67452301)'

    style = style_table.GetNextStyle()
    assert style == 'SYMBOL(id:"http://style1_normal",c:#67452301)'
    style_name = style_table.GetLastStyleName()
    assert style_name == 'style1_normal'

    style = style_table.GetNextStyle()
    assert style is None

    style_table.ResetStyleStringReading()
    style = style_table.GetNextStyle()
    assert style is not None

    # GetStyleTable()/SetStyleTable() on data source
    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    assert ds.GetStyleTable() is None
    ds.SetStyleTable(None)
    assert ds.GetStyleTable() is None
    ds.SetStyleTable(style_table)
    style_table2 = ds.GetStyleTable()
    style = style_table2.GetNextStyle()
    assert style == 'SYMBOL(id:"http://style1_normal",c:#67452301)'

    # GetStyleTable()/SetStyleTable() on layer
    lyr = ds.CreateLayer('foo')
    assert lyr.GetStyleTable() is None
    lyr.SetStyleTable(None)
    assert lyr.GetStyleTable() is None
    lyr.SetStyleTable(style_table)
    style_table2 = lyr.GetStyleTable()
    style = style_table2.GetNextStyle()
    assert style == 'SYMBOL(id:"http://style1_normal",c:#67452301)'

    ds = None

###############################################################################
# Build tests runner



