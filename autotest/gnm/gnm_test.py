#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
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

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import shutil

from osgeo import gdal
from osgeo import gnm

###############################################################################
# Create file base network

def gnm_filenetwork_create():

    try:
        shutil.rmtree('tmp/test_gnm')
    except:
        pass
        
    ogrtest.drv = None
    ogrtest.have_gnm = 0
        
    try:
        ogrtest.drv = gdal.GetDriverByName('GNMFile')
    except:
        pass

    if ogrtest.drv is None:
        return 'skip'    

    ds = ogrtest.drv.Create( 'tmp/', 0, 0, 0, gdal.GDT_Unknown, options = ['net_name=test_gnm', 'net_description=Test file based GNM', 'net_srs=EPSG:4326'] )
    # cast to GNM
    dn = gnm.CastToNetwork(ds)
    if dn is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if dn.GetVersion() != 100:
        gdaltest.post_reason( 'GNM: Check GNM version failed')
        return 'fail'
    if dn.GetName() != 'test_gnm':
        gdaltest.post_reason( 'GNM: Check GNM name failed')
        return 'fail'
    if dn.GetDescription() != 'Test file based GNM':
        gdaltest.post_reason( 'GNM: Check GNM description failed')
        return 'fail'

    dn = None
    ogrtest.have_gnm = 1
    return 'success'

###############################################################################
# Open file base network

def gnm_filenetwork_open():
    
    if not ogrtest.have_gnm:
        return 'skip'

    ds = gdal.OpenEx( 'tmp/test_gnm' )
    # cast to GNM
    dn = gnm.CastToNetwork(ds)
    if dn is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if dn.GetVersion() != 100:
        gdaltest.post_reason( 'GNM: Check GNM version failed')
        return 'fail'
    if dn.GetName() != 'test_gnm':
        gdaltest.post_reason( 'GNM: Check GNM name failed')
        return 'fail'
    if dn.GetDescription() != 'Test file based GNM':
        gdaltest.post_reason( 'GNM: Check GNM description failed')
        return 'fail'

    dn = None
    return 'success'

###############################################################################
# Import layers into file base network

def gnm_import():
    
    if not ogrtest.have_gnm:
        return 'skip'

    ds = gdal.OpenEx( 'tmp/test_gnm' )

    #pipes
    dspipes = gdal.OpenEx('data/pipes.shp', gdal.OF_VECTOR)
    lyrpipes = dspipes.GetLayerByIndex(0)
    new_lyr = ds.CopyLayer(lyrpipes, 'pipes')
    if new_lyr is None:
        gdaltest.post_reason('failed to import pipes')
        return 'fail'
    dspipes = None

    #wells
    dswells = gdal.OpenEx('data/wells.shp', gdal.OF_VECTOR)
    lyrwells = dswells.GetLayerByIndex(0)
    new_lyr = ds.CopyLayer(lyrwells, 'wells')
    if new_lyr is None:
        gdaltest.post_reason('failed to import wells')
        return 'fail'
    dswells = None

    if ds.GetLayerCount() != 2:
        gdaltest.post_reason('expected 2 layers')
        return 'fail'

    ds = None
    return 'success'

###############################################################################
# autoconnect

def gnm_autoconnect():

    if not ogrtest.have_gnm:
        return 'skip'
        
    ds = gdal.OpenEx( 'tmp/test_gnm' )
    dgn = gnm.CastToGenericNetwork(ds)
    if dgn is None:
        gdaltest.post_reason('cast to GNMGenericNetwork failed')
        return 'fail'

    ret = dgn.ConnectPointsByLines(['pipes', 'wells'], 0.000001, 1, 1, gnm.GNM_EDGE_DIR_BOTH)
    if ret != 0:
        gdaltest.post_reason('failed to connect')
        return 'fail'

    dgn = None
    return 'success'

###############################################################################
# Dijkstra shortest path

def gnm_graph_dijkstra():

    if not ogrtest.have_gnm:
        return 'skip'
        
    ds = gdal.OpenEx( 'tmp/test_gnm' )
    dn = gnm.CastToNetwork(ds)
    if dn is None:
        gdaltest.post_reason('cast to GNMNetwork failed')
        return 'fail'

    lyr = dn.GetPath(61, 50, gnm.GATDijkstraShortestPath)
    if lyr is None:
        gdaltest.post_reason('failed to get path')
        return 'fail'

    if lyr.GetFeatureCount() == 0:
        gdaltest.post_reason('failed to get path')
        return 'fail'

    dn.ReleaseResultSet(lyr)
    dn = None
    return 'success'
import ogrtest
###############################################################################
# KShortest Paths

def gnm_graph_kshortest():

    if not ogrtest.have_gnm:
        return 'skip'
        
    ds = gdal.OpenEx( 'tmp/test_gnm' )
    dn = gnm.CastToNetwork(ds)
    if dn is None:
        gdaltest.post_reason('cast to GNMNetwork failed')
        return 'fail'

    lyr = dn.GetPath(61, 50, gnm.GATKShortestPath, options = ['num_paths=3'])
    if lyr is None:
        gdaltest.post_reason('failed to get path')
        return 'fail'

    if lyr.GetFeatureCount() < 20:
        gdaltest.post_reason('failed to get path')
        return 'fail'

    dn.ReleaseResultSet(lyr)
    dn = None
    return 'success'

###############################################################################
# ConnectedComponents

def gnm_graph_connectedcomponents():

    if not ogrtest.have_gnm:
        return 'skip'
        
    ds = gdal.OpenEx( 'tmp/test_gnm' )
    dn = gnm.CastToNetwork(ds)
    if dn is None:
        gdaltest.post_reason('cast to GNMNetwork failed')
        return 'fail'

    lyr = dn.GetPath(61, 50, gnm.GATConnectedComponents)
    if lyr is None:
        gdaltest.post_reason('failed to get path')
        return 'fail'

    if lyr.GetFeatureCount() == 0:
        dn.ReleaseResultSet(lyr)
        gdaltest.post_reason('failed to get path')
        return 'fail'

    dn.ReleaseResultSet(lyr)
    dn = None
    return 'success'

###############################################################################
# Network deleting

def gnm_delete(): 

    if not ogrtest.have_gnm:
        return 'skip'
        
    gdal.GetDriverByName('GNMFile').Delete('tmp/test_gnm')

    try:
        os.stat('tmp/test_gnm')
        gdaltest.post_reason('Expected delete tmp/test_gnm')
        return 'fail'
    except:
        pass

    return 'success'
           
           

gnmtest_list = [
    gnm_filenetwork_create,
    gnm_filenetwork_open,
    gnm_import,
    gnm_autoconnect,
    gnm_graph_dijkstra,
    gnm_graph_kshortest,
    gnm_graph_connectedcomponents,
    gnm_delete
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'gnm_test' )

    gdaltest.run_tests( gnmtest_list )

    gdaltest.summarize()

