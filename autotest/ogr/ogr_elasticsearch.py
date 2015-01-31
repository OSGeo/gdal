#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ElasticSearch driver testing (with fake server)
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

import sys

sys.path.append( '../pymod' )

import ogrtest
import gdaltest
from osgeo import gdal
from osgeo import ogr
import webserver

###############################################################################
# Test driver availability
#

def ogr_elasticsearch_init():

    ogrtest.elasticsearch_drv = None

    try:
        ogrtest.elasticsearch_drv = ogr.GetDriverByName('ElasticSearch')
    except:
        pass
        
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    return 'success'

###############################################################################
# Test writing into an inexisting ElasticSearch datastore !

def ogr_elasticsearch_unexisting_server():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogrtest.elasticsearch_drv.CreateDataSource("http://127.0.0.1:6969")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('managed to open inexisting ElasticSearch datastore !')
        return 'fail'

    return 'success'

###############################################################################
# Test writing features into a local fake WFS server

def ogr_elasticsearch_fake_server():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    (process, port) = webserver.launch()
    if port == 0:
        return 'skip'

    gdal.SetConfigOption('ES_OVERWRITE', '1')
    ds = ogrtest.elasticsearch_drv.CreateDataSource("http://127.0.0.1:%d/fakeelasticsearch" % port)
    gdal.SetConfigOption('ES_OVERWRITE', None)
    if ds is None:
        gdaltest.post_reason('did not managed to open ElasticSearch datastore')
        webserver.server_stop(process, port)
        return 'fail'

    lyr = ds.CreateLayer('foo')
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    lyr = ds.CreateLayer('foo2')
    lyr.CreateField(ogr.FieldDefn('str_field', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('int_field', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('real_field', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('real_field_unset', ogr.OFTReal))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('str_field', 'a')
    feat.SetField('int_field', 1)
    feat.SetField('real_field', 2.34)
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 1)'))
    lyr.CreateFeature(feat)
    feat = None

    webserver.server_stop(process, port)

    return 'success'

gdaltest_list = [
    ogr_elasticsearch_init,
    ogr_elasticsearch_unexisting_server,
    ogr_elasticsearch_fake_server,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_elasticsearch' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

