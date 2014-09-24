#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Style testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import gdal

###############################################################################
#
#
def ogr_style_styletable():

    style_table = ogr.StyleTable()
    style_table.AddStyle("style1_normal", 'SYMBOL(id:"http://style1_normal",c:#67452301)')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = style_table.SaveStyleTable('/nonexisting')
    gdal.PopErrorHandler()
    if ret != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    if style_table.SaveStyleTable("/vsimem/out.txt") != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    style_table = None

    style_table = ogr.StyleTable()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = style_table.LoadStyleTable('/nonexisting')
    gdal.PopErrorHandler()
    if ret != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    if style_table.LoadStyleTable('/vsimem/out.txt') != 1:
        gdaltest.post_reason('failure')
        return 'fail'

    gdal.Unlink('/vsimem/out.txt')

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = style_table.Find("non_existing_style")
    gdal.PopErrorHandler()
    if ret is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    if style_table.Find("style1_normal") != 'SYMBOL(id:"http://style1_normal",c:#67452301)':
        gdaltest.post_reason('failure')
        return 'fail'

    style = style_table.GetNextStyle()
    if style != 'SYMBOL(id:"http://style1_normal",c:#67452301)':
        gdaltest.post_reason('failure')
        return 'fail'
    style_name = style_table.GetLastStyleName()
    if style_name != 'style1_normal':
        gdaltest.post_reason('failure')
        return 'fail'

    style = style_table.GetNextStyle()
    if style is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    style_table.ResetStyleStringReading()
    style = style_table.GetNextStyle()
    if style is None:
        gdaltest.post_reason('failure')
        return 'fail'

    # GetStyleTable()/SetStyleTable() on data source
    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    if ds.GetStyleTable() is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    ds.SetStyleTable(None)
    if ds.GetStyleTable() is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    ds.SetStyleTable(style_table)
    style_table2 = ds.GetStyleTable()
    style = style_table2.GetNextStyle()
    if style != 'SYMBOL(id:"http://style1_normal",c:#67452301)':
        gdaltest.post_reason('failure')
        return 'fail'

    # GetStyleTable()/SetStyleTable() on layer
    lyr = ds.CreateLayer('foo')
    if lyr.GetStyleTable() is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr.SetStyleTable(None)
    if lyr.GetStyleTable() is not None:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr.SetStyleTable(style_table)
    style_table2 = lyr.GetStyleTable()
    style = style_table2.GetNextStyle()
    if style != 'SYMBOL(id:"http://style1_normal",c:#67452301)':
        gdaltest.post_reason('failure')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Build tests runner

gdaltest_list = [ 
    ogr_style_styletable ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_style' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

