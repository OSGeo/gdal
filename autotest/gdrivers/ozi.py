#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  OZI Testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test reading OZF2 file

def ozi_online_1():

    if not gdaltest.download_file('http://www.oziexplorer2.com/maps/Europe2001_setup.exe', 'Europe2001_setup.exe'):
        return 'skip'

    try:
        os.stat('tmp/cache/Europe 2001_OZF.map')
    except:
        try:
            gdaltest.unzip('tmp/cache', 'tmp/cache/Europe2001_setup.exe')
            try:
                os.stat('tmp/cache/Europe 2001_OZF.map')
            except:
                return 'skip'
        except:
            return 'skip'

    ds = gdal.Open('tmp/cache/Europe 2001_OZF.map')
    if ds is None:
        return 'fail'

    if False:
        gt = ds.GetGeoTransform()
        wkt = ds.GetProjectionRef()

        expected_gt = (-1841870.2731215316, 3310.9550245520159, -13.025246304875619, 8375316.4662204208, -16.912440131236657, -3264.1162527118681)
        for i in range(6):
            if abs(gt[i] - expected_gt[i]) > 1e-7:
                gdaltest.post_reason('bad geotransform')
                print(gt)
                print(expected_gt)
                return 'fail'

    else:
        gcps = ds.GetGCPs()

        if len(gcps) != 4:
            gdaltest.post_reason( 'did not get expected gcp count.')
            print(len(gcps))
            return 'fail'

        gcp0 = gcps[0]
        if gcp0.GCPPixel != 61 or gcp0.GCPLine != 436 \
                or abs(gcp0.GCPX- (-1653990.4525324)) > 0.001 \
                or abs(gcp0.GCPY- 6950885.0402214) > 0.001:
            gdaltest.post_reason( 'did not get expected gcp.')
            print(gcp0)
            return 'fail'

        wkt = ds.GetGCPProjection()

    expected_wkt = 'PROJCS["unnamed",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]],PROJECTION["Lambert_Conformal_Conic_2SP"],PARAMETER["standard_parallel_1",40],PARAMETER["standard_parallel_2",56],PARAMETER["latitude_of_origin",4],PARAMETER["central_meridian",10],PARAMETER["false_easting",0],PARAMETER["false_northing",0],UNIT["Meter",1]]'
    if wkt != expected_wkt:
        gdaltest.post_reason('bad WKT')
        print(wkt)
        return 'fail'



    cs = ds.GetRasterBand(1).Checksum()
    if cs != 16025:
        gdaltest.post_reason('bad checksum')
        print(cs)
        return 'fail'

    return 'success'

gdaltest_list = [
    ozi_online_1 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'OZI' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

