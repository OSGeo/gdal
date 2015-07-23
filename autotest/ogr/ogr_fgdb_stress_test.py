#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  FGDB driver stress testing of CreateFeature() with user set FID
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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


import random
import sys
import shutil
import os

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr

###############################################################################
# Test if driver is available

def ogr_fgdb_stress_test_init():

    ogrtest.fgdb_drv = None
    ogrtest.openfilegdb_drv = None

    try:
        ogrtest.fgdb_drv = ogr.GetDriverByName('FileGDB')
    except:
        pass

    try:
        ogrtest.reference_drv = ogr.GetDriverByName('GPKG')
    except:
        ogrtest.reference_drv = None

    ogrtest.reference_ext = 'gpkg'

    try:
        ogrtest.openfilegdb_drv = ogr.GetDriverByName('OpenFileGDB')
    except:
        pass
    
    if ogrtest.fgdb_drv is None:
        return 'skip'
    if ogrtest.reference_drv is None:
        return 'skip'
    if ogrtest.openfilegdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    try:
        os.unlink("tmp/test." + ogrtest.reference_ext)
    except:
        pass

    return 'success'

###############################################################################
# Generate databases from random operations

def ogr_fgdb_stress_test_1():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    verbose = False

    ds_test = ogrtest.fgdb_drv.CreateDataSource('tmp/test.gdb')
    ds_ref = ogrtest.reference_drv.CreateDataSource('tmp/test.' + ogrtest.reference_ext)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr_test = ds_test.CreateLayer("test", geom_type = ogr.wkbPoint, srs = sr)
    lyr_ref = ds_ref.CreateLayer("test", geom_type = ogr.wkbPoint, srs = sr)
    for lyr in [lyr_test, lyr_ref]:
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    ds_test.ExecuteSQL("CREATE INDEX idx_test_str ON test(str)")
    ds_ref.ExecuteSQL("CREATE INDEX idx_test_str ON test(str)")
    random.seed(0)
    in_transaction = False
    nfeatures_created = 0
    for i in range(100000):
        #print(i)
        function = random.randrange(0,500)
        if function == 0:
            if not in_transaction:
                if verbose: print('StartTransaction')
                ds_test.StartTransaction(force = 1)
            else:
                if verbose: print('CommitTransaction')
                ds_test.CommitTransaction()
            in_transaction = not in_transaction
        elif function < 500 / 3:
            ret = []
            fid = -1
            if random.randrange(0,2) == 0:
                fid = 1+random.randrange(0,1000)
            str = '%d' % random.randrange(0,1000)
            wkt = 'POINT (%d %d)' % (random.randrange(0,100),random.randrange(0,100))
            if verbose: print('Create(%d)' % fid)
            for lyr in [lyr_test, lyr_ref]:
                f = ogr.Feature(lyr.GetLayerDefn())
                f.SetFID(fid)
                f.SetField(0, str)
                f.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
                gdal.PushErrorHandler()
                ret.append(lyr.CreateFeature(f))
                gdal.PopErrorHandler()
                # So to ensure lyr_ref will use the same FID as the tested layer
                fid = f.GetFID()
                #print("created %d" % fid)
            if ret[0] != ret[1]:
                gdaltest.post_reason('fail')
                print(ret)
                return 'fail'
            if ret[0] == 0:
                nfeatures_created += 1
        # For some odd reason, the .spx file is no longer updated when doing
        # a SetFeature() before having creating at least 2 features !
        elif function < 500 * 2 / 3 and nfeatures_created >= 2:
            ret = []
            fid = 1+random.randrange(0,1000)
            if verbose: print('Update(%d)' % fid)
            str = '%d' % random.randrange(0,1000)
            wkt = 'POINT (%d %d)' % (random.randrange(0,100),random.randrange(0,100))
            for lyr in [lyr_test, lyr_ref]:
                f = ogr.Feature(lyr.GetLayerDefn())
                f.SetFID(fid)
                f.SetField(0, str)
                f.SetGeometry(ogr.CreateGeometryFromWkt(wkt))
                #gdal.PushErrorHandler()
                ret.append(lyr.SetFeature(f))
                #gdal.PopErrorHandler()
            if ret[0] != ret[1]:
                gdaltest.post_reason('fail')
                print(ret)
                return 'fail'
        # Same for DeleteFeature()
        elif nfeatures_created >= 2:
            ret = []
            fid = 1+random.randrange(0,1000)
            if verbose: print('Delete(%d)' % fid)
            for lyr in [lyr_test, lyr_ref]:
                #gdal.PushErrorHandler()
                ret.append(lyr.DeleteFeature(fid))
                #gdal.PopErrorHandler()
            if ret[0] != ret[1]:
                gdaltest.post_reason('fail')
                print(ret)
                return 'fail'
            
    if in_transaction:
        ds_test.CommitTransaction()
    
    return 'success'

###############################################################################
# Compare databases

def ogr_fgdb_stress_test_2():
    if ogrtest.fgdb_drv is None:
        return 'skip'
    
    ds_test = ogr.Open('tmp/test.gdb')
    ds_ref = ogr.Open('tmp/test.' + ogrtest.reference_ext)
    
    lyr_test = ds_test.GetLayer(0)
    lyr_ref = ds_ref.GetLayer(0)
    
    while True:
        f_test = lyr_test.GetNextFeature()
        f_ref = lyr_ref.GetNextFeature()
        if (f_test is None and f_ref is not None) or (f_test is not None and f_ref is None):
            gdaltest.post_reason('fail')
            return 'fail'
        if f_test is None:
            break
        if f_test.GetFID() != f_ref.GetFID() or \
           f_test['str'] != f_ref['str'] or \
           ogrtest.check_feature_geometry(f_test, f_ref.GetGeometryRef()) != 0:
            gdaltest.post_reason('fail')
            f_test.DumpReadable()
            f_ref.DumpReadable()
            return 'fail'
    
    for val in range(1000):
        lyr_test.SetAttributeFilter("str = '%d'" % val)
        lyr_ref.SetAttributeFilter("str = '%d'" % val)
        if lyr_test.GetFeatureCount() != lyr_ref.GetFeatureCount():
            gdaltest.post_reason('fail')
            print(val)
            print(lyr_test.GetFeatureCount())
            print(lyr_ref.GetFeatureCount())
            return 'fail'

    #sys.exit(0)
    
    return 'success'
            

###############################################################################
# Cleanup

def ogr_fgdb_stress_test_cleanup():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    try:
        os.unlink("tmp/test." + ogrtest.reference_ext)
    except:
        pass

    return 'success'


explicit_gdaltest_list = [ ogr_fgdb_stress_test_init,
                           ogr_fgdb_stress_test_1,
                           ogr_fgdb_stress_test_2,
                           ogr_fgdb_stress_test_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_fgdb_stress_test' )

    gdaltest.run_tests( explicit_gdaltest_list )

    gdaltest.summarize()



