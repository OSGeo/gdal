#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Selafin driver testing.
# Author:   François Hissel <francois.hissel@gmail.com>
# 
###############################################################################
# Copyright (c) 2014, François Hissel <francois.hissel@gmail.com>
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
import string
import math

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr, osr, gdal

###############################################################################
# Create wasp datasource

def ogr_selafin_create_ds():

    gdaltest.selafin_ds = None
    try:
        os.remove( 'tmp/tmp.slf' )
    except:
        pass
    selafin_drv = ogr.GetDriverByName('Selafin')
    
    gdaltest.selafin_ds = selafin_drv.CreateDataSource( 'tmp/tmp.slf' )


    if gdaltest.selafin_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Add a few points to the datasource

def ogr_selafin_create_nodes():
    if ogr_selafin_create_ds() != 'success': return 'skip'
    ref=osr.SpatialReference()
    ref.ImportFromEPSG(4326)
    layer=gdaltest.selafin_ds.CreateLayer("name",ref,geom_type=ogr.wkbPoint)
    if layer == None:
        gdaltest.post_reason( 'unable to create layer')
        return 'fail'
    layer.CreateField(ogr.FieldDefn("value",ogr.OFTReal))
    dfn=layer.GetLayerDefn()
    for i in range(5):
        for j in range(5):
            pt=ogr.Geometry(type=ogr.wkbPoint)
            pt.AddPoint_2D(float(i),float(j))
            feat=ogr.Feature(dfn)
            feat.SetGeometry(pt)
            feat.SetField(0,(float)(i*5+j))
            if layer.CreateFeature(feat)!=0:
                gdaltest.post_reason( 'unable to create node feature')
                return 'fail'
    # do some checks
    if layer.GetFeatureCount()!=25:
        gdaltest.post_reason("wrong number of features after point layer creation")
        return "fail"
    # return
    del gdaltest.selafin_ds
    del layer
    return "success"

###############################################################################
# Add a set of elements to the datasource

def ogr_selafin_create_elements():

    gdaltest.selafin_ds = ogr.Open( 'tmp/tmp.slf',1 )
    if gdaltest.selafin_ds is None:
        return "skip"
    layerCount=gdaltest.selafin_ds.GetLayerCount()
    if layerCount<2:
        gdaltest.post_reason("elements layer not created with nodes layer")
        return "fail"
    for i in range(layerCount):
        name=gdaltest.selafin_ds.GetLayer(i).GetName()
        if "_e" in name:
            j=i
        if "_p" in name:
            k=i
    layere=gdaltest.selafin_ds.GetLayer(j)
    dfn=layere.GetLayerDefn()
    for i in range(4):
        for j in range(4):
            pol=ogr.Geometry(type=ogr.wkbPolygon)
            poll=ogr.Geometry(type=ogr.wkbLinearRing)
            poll.AddPoint_2D(float(i),float(j))
            poll.AddPoint_2D(float(i),float(j+1))
            poll.AddPoint_2D(float(i+1),float(j+1))
            poll.AddPoint_2D(float(i+1),float(j))
            poll.AddPoint_2D(float(i),float(j))
            pol.AddGeometry(poll)
            feat=ogr.Feature(dfn)
            feat.SetGeometry(pol)
            if layere.CreateFeature(feat)!=0:
                gdaltest.post_reason( 'unable to create element feature')
                return 'fail'
    pol=ogr.Geometry(type=ogr.wkbPolygon)
    poll=ogr.Geometry(type=ogr.wkbLinearRing)
    poll.AddPoint_2D(4.0,4.0)
    poll.AddPoint_2D(4.0,5.0)
    poll.AddPoint_2D(5.0,5.0)
    poll.AddPoint_2D(5.0,4.0)
    poll.AddPoint_2D(4.0,4.0)
    pol.AddGeometry(poll)
    feat=ogr.Feature(dfn)
    feat.SetGeometry(pol)
    if layere.CreateFeature(feat)!=0:
        gdaltest.post_reason( 'unable to create element feature')
        return 'fail'
    # do some checks
    if gdaltest.selafin_ds.GetLayer(k).GetFeatureCount()!=28:
        gdaltest.post_reason("wrong number of point features after elements layer creation")
        return "fail"
    if math.fabs(layere.GetFeature(5).GetFieldAsDouble(0)-9)>0.01:
        gdaltest.post_reason("wrong value of attribute in element layer")
        return "fail"
    if math.fabs(layere.GetFeature(10).GetFieldAsDouble(0)-15)>0.01:
        gdaltest.post_reason("wrong value of attribute in element layer")
        return "fail"
    # return
    del gdaltest.selafin_ds
    return "success"

###############################################################################
# Add a field and set its values for point features
def ogr_selafin_set_field():

    gdaltest.selafin_ds = ogr.Open( 'tmp/tmp.slf',1 )
    if gdaltest.selafin_ds is None:
        return "skip"
    layerCount=gdaltest.selafin_ds.GetLayerCount()
    if layerCount<2:
        gdaltest.post_reason("elements layer not created with nodes layer")
        return "fail"
    for i in range(layerCount):
        name=gdaltest.selafin_ds.GetLayer(i).GetName()
        if "_e" in name:
            j=i
        if "_p" in name:
            k=i
    layern=gdaltest.selafin_ds.GetLayer(k)
    layere=gdaltest.selafin_ds.GetLayer(j)
    layern.CreateField(ogr.FieldDefn("reverse",ogr.OFTReal))
    layern.AlterFieldDefn(0,ogr.FieldDefn("new",ogr.OFTReal),ogr.ALTER_NAME_FLAG)
    layern.ReorderFields([1,0])
    dfn=layern.GetLayerDefn()
    for i in range(28):
        feat=layern.GetFeature(i)
        val=feat.GetFieldAsDouble(1)
        feat.SetField(0,(float)(val*10))
        layern.SetFeature(feat)
    # do some checks
    if math.fabs(layern.GetFeature(11).GetFieldAsDouble(0)-110)>0.01:
        gdaltest.post_reason("wrong value of attribute in point layer")
        return "fail"
    # return
    del gdaltest.selafin_ds
    return "success"


###############################################################################
# Cleanup

def ogr_selafin_cleanup():

    selafin_drv = ogr.GetDriverByName('Selafin')
    selafin_drv.DeleteDataSource( 'tmp/tmp.slf' )
    return 'success'

gdaltest_list = [ 
    ogr_selafin_create_ds,
    ogr_selafin_create_nodes,
    ogr_selafin_create_elements,
    ogr_selafin_set_field,
    ogr_selafin_cleanup
    ]

if __name__ == '__main__':
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ogrtest.have_geos() 
    gdal.PopErrorHandler()

    gdaltest.setup_run( 'ogr_selafin' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

