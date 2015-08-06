#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsistdin/
# Author:   Even Rouault <even dot rouault at mines dash parid dot org>
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

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal
import test_cli_utilities

###############################################################################
# Test on a small file

def vsistdin_1():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vsistdin_1_src.tif', src_ds)
    ds = None
    cs = src_ds.GetRasterBand(1).Checksum()
    src_ds = None

    # Should work on both Unix and Windows
    os.system(test_cli_utilities.get_gdal_translate_path() + " /vsistdin/ tmp/vsistdin_1_out.tif -q < tmp/vsistdin_1_src.tif")

    try:
        os.unlink("tmp/vsistdin_1_src.tif")
    except:
        pass

    ds = gdal.Open("tmp/vsistdin_1_out.tif")
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetRasterBand(1).Checksum() != cs:
        gdaltest.post_reason('fail')
        return 'fail'

    try:
        os.unlink("tmp/vsistdin_1_out.tif")
    except:
        pass

    return 'success'

###############################################################################
# Test on a bigger file (> 1 MB)

def vsistdin_2():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    ds = gdal.GetDriverByName('GTiff').Create('tmp/vsistdin_2_src.tif', 2048, 2048)
    ds = None

    # Should work on both Unix and Windows
    os.system(test_cli_utilities.get_gdal_translate_path() + " /vsistdin/ tmp/vsistdin_2_out.tif -q < tmp/vsistdin_2_src.tif")

    try:
        os.unlink("tmp/vsistdin_2_src.tif")
    except:
        pass
    
    ds = gdal.Open("tmp/vsistdin_2_out.tif")
    if ds is None:
        return 'fail'
    ds = None

    try:
        os.unlink("tmp/vsistdin_2_out.tif")
    except:
        pass

    return 'success'

###############################################################################
# Test opening /vsistdin/ in write mode (failure expected)

def vsistdin_3():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f = gdal.VSIFOpenL('/vsistdin/', 'wb')
    gdal.PopErrorHandler()
    if f is not None:
        return 'fail'

    return 'success'

###############################################################################
# Test fix for #6061

def vsistdin_4():
    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    f = open('tmp/vsistdin_4_src.vrt', 'wt')
    f.write("""<VRTDataset rasterXSize="20" rasterYSize="20">
  %s
  <SRS>PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982138982,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]]</SRS>
  <GeoTransform>  4.4072000000000000e+05,  6.0000000000000000e+01,  0.0000000000000000e+00,  3.7513200000000000e+06,  0.0000000000000000e+00, -6.0000000000000000e+01</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""" % (' '.join([' ' for i in range(1024*1024)])) )
    f.close()

    # Should work on both Unix and Windows
    os.system(test_cli_utilities.get_gdal_translate_path() + " /vsistdin/ tmp/vsistdin_4_out.tif -q < tmp/vsistdin_4_src.vrt")

    try:
        os.unlink("tmp/vsistdin_4_src.vrt")
    except:
        pass
    
    ds = gdal.Open("tmp/vsistdin_4_out.tif")
    if ds is None:
        return 'fail'
    ds = None

    try:
        os.unlink("tmp/vsistdin_4_out.tif")
    except:
        pass

    return 'success'

gdaltest_list = [ vsistdin_1,
                  vsistdin_2,
                  vsistdin_3,
                  vsistdin_4
                ]


if __name__ == '__main__':

    gdaltest.setup_run( 'vsistdin' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
