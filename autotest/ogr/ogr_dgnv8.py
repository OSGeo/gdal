#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test DGNv8 Driver.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault <even dot rouault at spatialys dot com>
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
import shutil
import sys

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal, ogr

###############################################################################
# Verify we can open the test file.

def ogr_dgnv8_1():

    gdaltest.dgnv8_drv = ogr.GetDriverByName('DGNv8')
    if gdaltest.dgnv8_drv is None:
        return 'skip'

    ds = ogr.Open( 'data/test_dgnv8.dgn' )
    if ds is None:
        gdaltest.post_reason( 'failed to open test file.' )
        return 'fail'

    return 'success'

###############################################################################
# Compare with a reference CSV dump

def ogr_dgnv8_2():

    if gdaltest.dgnv8_drv is None:
        return 'skip'

    gdal.VectorTranslate('/vsimem/ogr_dgnv8_2.csv', 'data/test_dgnv8.dgn',
        options = '-f CSV  -dsco geometry=as_wkt -sql "select *, ogr_style from my_model"')

    ds_ref = ogr.Open('/vsimem/ogr_dgnv8_2.csv')
    lyr_ref = ds_ref.GetLayer(0)
    ds = ogr.Open( 'data/test_dgnv8_ref.csv' )
    lyr = ds.GetLayer(0)
    ret = ogrtest.compare_layers(lyr, lyr_ref, excluded_fields = ['WKT'])
       
    gdal.Unlink('/vsimem/ogr_dgnv8_2.csv')
        
    return ret

###############################################################################
# Run test_ogrsf

def ogr_dgnv8_3():

    if gdaltest.dgnv8_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test_dgnv8.dgn')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    shutil.copy( 'data/test_dgnv8.dgn', 'tmp/test_dgnv8.dgn' )
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/test_dgnv8.dgn')
    os.unlink('tmp/test_dgnv8.dgn')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test creation code

def ogr_dgnv8_4():

    if gdaltest.dgnv8_drv is None:
        return 'skip'

    tmp_dgn = 'tmp/ogr_dgnv8_4.dgn'
    gdal.VectorTranslate(tmp_dgn, 'data/test_dgnv8.dgn', format = 'DGNv8')

    tmp_csv = '/vsimem/ogr_dgnv8_4.csv'
    gdal.VectorTranslate(tmp_csv, tmp_dgn,
        options = '-f CSV  -dsco geometry=as_wkt -sql "select *, ogr_style from my_model"')
    gdal.Unlink(tmp_dgn)

    ds_ref = ogr.Open(tmp_csv)
    lyr_ref = ds_ref.GetLayer(0)
    ds = ogr.Open( 'data/test_dgnv8_write_ref.csv' )
    lyr = ds.GetLayer(0)
    ret = ogrtest.compare_layers(lyr, lyr_ref, excluded_fields = ['WKT'])
       
    gdal.Unlink(tmp_csv)
        
    return ret

###############################################################################
# Test creation options

def ogr_dgnv8_5():

    if gdaltest.dgnv8_drv is None:
        return 'skip'

    tmp_dgn = 'tmp/ogr_dgnv8_5.dgn'
    options = [ 'APPLICATION=application',
                'TITLE=title',
                'SUBJECT=subject',
                'AUTHOR=author',
                'KEYWORDS=keywords',
                'TEMPLATE=template',
                'COMMENTS=comments',
                'LAST_SAVED_BY=last_saved_by',
                'REVISION_NUMBER=revision_number',
                'CATEGORY=category',
                'MANAGER=manager',
                'COMPANY=company' ]
    ds = gdaltest.dgnv8_drv.CreateDataSource(tmp_dgn, options = options)
    lyr = ds.CreateLayer('my_layer')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 1)'))
    lyr.CreateFeature(f)
    ds = None
    ds = ogr.Open(tmp_dgn)
    got_md = ds.GetMetadata_List('DGN')
    if got_md != options:
        gdaltest.post_reason('fail')
        print(got_md)
        return 'fail'
    ds = None
    
    tmp2_dgn = 'tmp/ogr_dgnv8_5_2.dgn'
    gdaltest.dgnv8_drv.CreateDataSource(tmp2_dgn, options = ['SEED=' + tmp_dgn, 'TITLE=another_title'])
    ds = ogr.Open(tmp2_dgn)
    if ds.GetMetadataItem('TITLE', 'DGN') != 'another_title' or ds.GetMetadataItem('APPLICATION', 'DGN') != 'application':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata('DGN'))
        return 'fail'
    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'my_layer':
        gdaltest.post_reason('fail')
        print(lyr.GetName())
        return 'fail'
    if lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'
    ds = None

    ds = gdaltest.dgnv8_drv.CreateDataSource(tmp2_dgn, options = ['SEED=' + tmp_dgn])
    lyr = ds.CreateLayer('a_layer', options = ['DESCRIPTION=my_layer', 'DIM=2'])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 3)'))
    lyr.CreateFeature(f)
    ds = None
    ds = ogr.Open(tmp2_dgn, update = 1)
    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'a_layer':
        gdaltest.post_reason('fail')
        print(lyr.GetName())
        return 'fail'
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 3)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None
    
    gdal.Unlink(tmp_dgn)
    gdal.Unlink(tmp2_dgn)
        
    return 'success'


###############################################################################
#  Cleanup

def ogr_dgnv8_cleanup():

    return 'success'

gdaltest_list = [
    ogr_dgnv8_1,
    ogr_dgnv8_2,
    ogr_dgnv8_3,
    ogr_dgnv8_4,
    ogr_dgnv8_5,
    ogr_dgnv8_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_dgnv8' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
