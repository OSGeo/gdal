#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VRTWarpedDataset support.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
import test_cli_utilities

###############################################################################
# Verify reading from simple existing warp definition.

def vrtwarp_1():

    tst = gdaltest.GDALTest( 'VRT', 'rgb_warp.vrt', 2, 21504 )
    return tst.testOpen()

###############################################################################
# Create a new VRT warp in the temp directory.

def vrtwarp_2():

    try:
        os.remove( 'tmp/warp.vrt' )
    except:
        pass
    
    gcp_ds = gdal.OpenShared( 'data/rgb_gcp.vrt', gdal.GA_ReadOnly )

    gdaltest.vrtwarp_ds = gdal.AutoCreateWarpedVRT( gcp_ds )

    gcp_ds = None
    
    checksum = gdaltest.vrtwarp_ds.GetRasterBand(2).Checksum()
    expected = 21504
    if checksum != expected:
        gdaltest.post_reason( 'Got checksum of %d instead of expected %d.' \
                              % (checksum, expected) )
        return 'fail'
        
    return 'success'

###############################################################################
# Force the VRT warp file to be written to disk and close it.  Reopen, and
# verify checksum.

def vrtwarp_3():

    gdaltest.vrtwarp_ds.SetDescription( 'tmp/warp.vrt' )
    gdaltest.vrtwarp_ds = None

    gdaltest.vrtwarp_ds = gdal.Open( 'tmp/warp.vrt', gdal.GA_ReadOnly )

    checksum = gdaltest.vrtwarp_ds.GetRasterBand(2).Checksum()
    expected = 21504

    gdaltest.vrtwarp_ds = None
    gdal.GetDriverByName('VRT').Delete( 'tmp/warp.vrt' )

    if checksum != expected:
        gdaltest.post_reason( 'Got checksum of %d instead of expected %d.' \
                              % (checksum, expected) )
        return 'fail'

    return 'success'

###############################################################################
# Test implicit overviews with default source overview level strategy (AUTO)

def vrtwarp_4():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vrtwarp_4.tif', src_ds)
    cs_main = tmp_ds.GetRasterBand(1).Checksum()
    tmp_ds.BuildOverviews( 'NONE', overviewlist = [2, 4] )
    tmp_ds.GetRasterBand(1).GetOverview(0).Fill(127)
    cs_ov0 = tmp_ds.GetRasterBand(1).GetOverview(0).Checksum()
    tmp_ds.GetRasterBand(1).GetOverview(1).Fill(255)
    cs_ov1 = tmp_ds.GetRasterBand(1).GetOverview(1).Checksum()

    vrtwarp_ds = gdal.AutoCreateWarpedVRT( tmp_ds )
    tmp_ds = None
    
    for i in range(3):
        if vrtwarp_ds.GetRasterBand(1).GetOverviewCount() != 2:
            gdaltest.post_reason('fail')
            return 'fail'
        if vrtwarp_ds.GetRasterBand(1).Checksum() != cs_main:
            print(i)
            gdaltest.post_reason('fail')
            return 'fail'
        if vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() != cs_ov0:
            gdaltest.post_reason('fail')
            return 'fail'
        if vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() != cs_ov1:
            gdaltest.post_reason('fail')
            return 'fail'
        if i == 0:
            vrtwarp_ds.SetDescription( 'tmp/vrtwarp_4.vrt' )
            vrtwarp_ds = None
            vrtwarp_ds = gdal.Open( 'tmp/vrtwarp_4.vrt' )
        elif i == 1:
            vrtwarp_ds = None
            tmp_ds = gdal.Open('tmp/vrtwarp_4.tif')
            vrtwarp_ds = gdal.AutoCreateWarpedVRT( tmp_ds )
            vrtwarp_ds.SetMetadataItem('SrcOvrLevel', 'AUTO')
            vrtwarp_ds.SetDescription( 'tmp/vrtwarp_4.vrt' )
            tmp_ds = None

    
    # Add an explicit overview
    vrtwarp_ds.BuildOverviews( 'NEAR', overviewlist = [2, 4, 8] )
    vrtwarp_ds = None

    ds = gdal.GetDriverByName('MEM').Create('', 3, 3, 1)
    ds.GetRasterBand(1).Fill(255)
    expected_cs_ov2 = ds.GetRasterBand(1).Checksum()
    ds = None
    
    vrtwarp_ds = gdal.Open( 'tmp/vrtwarp_4.vrt' )
    if vrtwarp_ds.GetRasterBand(1).GetOverviewCount() != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).Checksum() != cs_main:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() != cs_ov0:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() != cs_ov1:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(2).Checksum() != expected_cs_ov2:
        gdaltest.post_reason('fail')
        return 'fail'
    vrtwarp_ds = None
    
    gdal.Unlink('tmp/vrtwarp_4.vrt')
    gdal.Unlink('tmp/vrtwarp_4.tif')

    return 'success'

###############################################################################
# Test implicit overviews with selection of the upper source overview level

def vrtwarp_5():
    
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vrtwarp_5.tif', src_ds)
    cs_main = tmp_ds.GetRasterBand(1).Checksum()
    tmp_ds.BuildOverviews( 'NONE', overviewlist = [2, 4] )
    tmp_ds.GetRasterBand(1).GetOverview(0).Fill(127)
    tmp_ds.GetRasterBand(1).GetOverview(0).Checksum()
    tmp_ds.GetRasterBand(1).GetOverview(1).Fill(255)
    tmp_ds.GetRasterBand(1).GetOverview(1).Checksum()
    tmp_ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' tmp/vrtwarp_5.tif -ovr NONE tmp/vrtwarp_5_tmp.tif -overwrite -ts 10 10')
    ds = gdal.Open('tmp/vrtwarp_5_tmp.tif')
    expected_cs_ov0 = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink('tmp/vrtwarp_5_tmp.tif')

    ds = gdal.GetDriverByName('MEM').Create('', 5, 5, 1)
    ds.GetRasterBand(1).Fill(127)
    expected_cs_ov1 = ds.GetRasterBand(1).Checksum()
    ds = None
    
    tmp_ds = gdal.Open('tmp/vrtwarp_5.tif')
    vrtwarp_ds = gdal.AutoCreateWarpedVRT( tmp_ds )
    vrtwarp_ds.SetMetadataItem('SrcOvrLevel', 'AUTO-1')
    tmp_ds = None
    if vrtwarp_ds.GetRasterBand(1).GetOverviewCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).Checksum() != cs_main:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() != expected_cs_ov0:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() != expected_cs_ov1:
        gdaltest.post_reason('fail')
        return 'fail'
    vrtwarp_ds = None

    gdal.Unlink('tmp/vrtwarp_5.tif')

    return 'success'

###############################################################################
# Test implicit overviews with GCP

def vrtwarp_6():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vrtwarp_6.tif', src_ds)
    cs_main = tmp_ds.GetRasterBand(1).Checksum()
    tmp_ds.SetGeoTransform([0,1,0,0,0,1]) # cancel geotransform
    gcp1 = gdal.GCP()
    gcp1.GCPPixel = 0
    gcp1.GCPLine = 0
    gcp1.GCPX = 440720.000
    gcp1.GCPY = 3751320.000
    gcp2 = gdal.GCP()
    gcp2.GCPPixel = 0
    gcp2.GCPLine = 20
    gcp2.GCPX = 440720.000
    gcp2.GCPY = 3750120.000
    gcp3 = gdal.GCP()
    gcp3.GCPPixel = 20
    gcp3.GCPLine = 0
    gcp3.GCPX = 441920.000
    gcp3.GCPY = 3751320.000
    src_gcps = (gcp1, gcp2, gcp3)
    tmp_ds.SetGCPs(src_gcps, src_ds.GetProjectionRef())
    tmp_ds.BuildOverviews( 'NEAR', overviewlist = [2, 4] )
    cs_ov0 = tmp_ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs_ov1 = tmp_ds.GetRasterBand(1).GetOverview(1).Checksum()

    vrtwarp_ds = gdal.AutoCreateWarpedVRT( tmp_ds )
    vrtwarp_ds.SetDescription( 'tmp/vrtwarp_6.vrt' )
    vrtwarp_ds = None
    tmp_ds = None
    
    vrtwarp_ds = gdal.Open( 'tmp/vrtwarp_6.vrt' )

    if vrtwarp_ds.GetRasterBand(1).GetOverviewCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).Checksum() != cs_main:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() != cs_ov0:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() != cs_ov1:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('tmp/vrtwarp_6.vrt')
    gdal.Unlink('tmp/vrtwarp_6.tif')

    return 'success'

###############################################################################
# Test implicit overviews with GCP (TPS)

def vrtwarp_7():
    
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'
    
    src_ds = gdal.Open('../gcore/data/byte.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vrtwarp_7.tif', src_ds)
    cs_main = tmp_ds.GetRasterBand(1).Checksum()
    tmp_ds.SetGeoTransform([0,1,0,0,0,1]) # cancel geotransform
    gcp1 = gdal.GCP()
    gcp1.GCPPixel = 0
    gcp1.GCPLine = 0
    gcp1.GCPX = 440720.000
    gcp1.GCPY = 3751320.000
    gcp2 = gdal.GCP()
    gcp2.GCPPixel = 0
    gcp2.GCPLine = 20
    gcp2.GCPX = 440720.000
    gcp2.GCPY = 3750120.000
    gcp3 = gdal.GCP()
    gcp3.GCPPixel = 20
    gcp3.GCPLine = 0
    gcp3.GCPX = 441920.000
    gcp3.GCPY = 3751320.000
    src_gcps = (gcp1, gcp2, gcp3)
    tmp_ds.SetGCPs(src_gcps, src_ds.GetProjectionRef())
    tmp_ds.BuildOverviews( 'NEAR', overviewlist = [2, 4] )
    cs_ov0 = tmp_ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs_ov1 = tmp_ds.GetRasterBand(1).GetOverview(1).Checksum()
    tmp_ds = None
    
    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' tmp/vrtwarp_7.tif tmp/vrtwarp_7.vrt -overwrite -of VRT -tps')
    
    vrtwarp_ds = gdal.Open( 'tmp/vrtwarp_7.vrt' )
    if vrtwarp_ds.GetRasterBand(1).GetOverviewCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).Checksum() != cs_main:
        print(cs_main)
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() != cs_ov0:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() != cs_ov1:
        gdaltest.post_reason('fail')
        return 'fail'
    vrtwarp_ds = None
    
    gdal.Unlink('tmp/vrtwarp_7.vrt')
    gdal.Unlink('tmp/vrtwarp_7.tif')

    return 'success'

###############################################################################
# Test implicit overviews with RPC

def vrtwarp_8():
    
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    import shutil
    shutil.copy('../gcore/data/byte.tif', 'tmp/vrtwarp_8.tif')
    shutil.copy('../gcore/data/test_rpc.txt', 'tmp/vrtwarp_8_rpc.txt')
    ds = gdal.Open('tmp/vrtwarp_8.tif', gdal.GA_Update)
    ds.BuildOverviews( 'NEAR', overviewlist = [2] )
    ds = None
   
    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' tmp/vrtwarp_8.tif tmp/vrtwarp_8_tmp.tif -overwrite -rpc')
    ds = gdal.Open('tmp/vrtwarp_8_tmp.tif')
    expected_cs_main = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink('tmp/vrtwarp_8_tmp.tif')

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' tmp/vrtwarp_8.tif tmp/vrtwarp_8.vrt -overwrite -of VRT -rpc')
    
    vrtwarp_ds = gdal.Open( 'tmp/vrtwarp_8.vrt' )
    if vrtwarp_ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).Checksum() != expected_cs_main:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() != 1189:
        gdaltest.post_reason('fail')
        print(vrtwarp_ds.GetRasterBand(1).GetOverview(0).XSize)
        print(vrtwarp_ds.GetRasterBand(1).GetOverview(0).YSize)
        print(vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum())
        return 'fail'
    vrtwarp_ds = None

    gdal.Unlink('tmp/vrtwarp_8.vrt')
    gdal.Unlink('tmp/vrtwarp_8.tif')
    gdal.Unlink('tmp/vrtwarp_8_rpc.txt')

    return 'success'

###############################################################################
# Test implicit overviews with GEOLOCATION

def vrtwarp_9():
    
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    import shutil
    shutil.copy('../gcore/data/sstgeo.tif', 'tmp/sstgeo.tif')

    
    f = open('tmp/sstgeo.vrt', 'wb')
    f.write('''<VRTDataset rasterXSize="60" rasterYSize="39">
  <Metadata domain="GEOLOCATION">
    <MDI key="SRS">GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AXIS["Lat",NORTH],AXIS["Long",EAST],AUTHORITY["EPSG","4326"]]</MDI>
    <MDI key="X_DATASET">tmp/sstgeo.tif</MDI>
    <MDI key="X_BAND">1</MDI>
    <MDI key="PIXEL_OFFSET">0</MDI>
    <MDI key="PIXEL_STEP">1</MDI>
    <MDI key="Y_DATASET">tmp/sstgeo.tif</MDI>
    <MDI key="Y_BAND">2</MDI>
    <MDI key="LINE_OFFSET">0</MDI>
    <MDI key="LINE_STEP">1</MDI>
  </Metadata>
  <VRTRasterBand dataType="Int16" band="1">
    <ColorInterp>Gray</ColorInterp>
    <NoDataValue>-32767</NoDataValue>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">sstgeo.tif</SourceFilename>
      <SourceBand>3</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="60" ySize="39"/>
      <DstRect xOff="0" yOff="0" xSize="60" ySize="39"/>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>
'''.encode('ascii'))
    f.close()
    ds = gdal.Open('tmp/sstgeo.vrt', gdal.GA_Update)
    ds.BuildOverviews( 'NEAR', overviewlist = [2] )
    ds = None
    
    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' tmp/sstgeo.vrt tmp/vrtwarp_9_tmp.tif -overwrite -geoloc')
    ds = gdal.Open('tmp/vrtwarp_9_tmp.tif')
    expected_cs_main = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink('tmp/vrtwarp_9_tmp.tif')

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' tmp/sstgeo.vrt tmp/vrtwarp_9.vrt -overwrite -of VRT -geoloc')
    
    vrtwarp_ds = gdal.Open( 'tmp/vrtwarp_9.vrt' )
    if vrtwarp_ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).Checksum() != expected_cs_main:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() != 63972:
        gdaltest.post_reason('fail')
        print(vrtwarp_ds.GetRasterBand(1).GetOverview(0).XSize)
        print(vrtwarp_ds.GetRasterBand(1).GetOverview(0).YSize)
        print(vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum())
        return 'fail'
    vrtwarp_ds = None

    gdal.Unlink('tmp/vrtwarp_9.vrt')
    gdal.Unlink('tmp/sstgeo.vrt')
    gdal.Unlink('tmp/sstgeo.vrt.ovr')
    gdal.Unlink('tmp/sstgeo.tif')

    return 'success'

###############################################################################
# Test implicit overviews with selection of the full resolution level

def vrtwarp_10():
    
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vrtwarp_10.tif', src_ds)
    cs_main = tmp_ds.GetRasterBand(1).Checksum()
    tmp_ds.BuildOverviews( 'NONE', overviewlist = [2, 4] )
    tmp_ds.GetRasterBand(1).GetOverview(0).Fill(127)
    tmp_ds.GetRasterBand(1).GetOverview(0).Checksum()
    tmp_ds.GetRasterBand(1).GetOverview(1).Fill(255)
    tmp_ds.GetRasterBand(1).GetOverview(1).Checksum()
    tmp_ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' tmp/vrtwarp_10.tif -ovr NONE tmp/vrtwarp_10_tmp.tif -overwrite -ts 10 10')
    ds = gdal.Open('tmp/vrtwarp_10_tmp.tif')
    expected_cs_ov0 = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink('tmp/vrtwarp_10_tmp.tif')

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' tmp/vrtwarp_10.tif -ovr NONE tmp/vrtwarp_10_tmp.tif -overwrite -ts 5 5')
    ds = gdal.Open('tmp/vrtwarp_10_tmp.tif')
    expected_cs_ov1 = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.Unlink('tmp/vrtwarp_10_tmp.tif')
    
    tmp_ds = gdal.Open('tmp/vrtwarp_10.tif')
    vrtwarp_ds = gdal.AutoCreateWarpedVRT( tmp_ds )
    vrtwarp_ds.SetMetadataItem('SrcOvrLevel', 'NONE')
    tmp_ds = None
    if vrtwarp_ds.GetRasterBand(1).GetOverviewCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).Checksum() != cs_main:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() != expected_cs_ov0:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() != expected_cs_ov1:
        gdaltest.post_reason('fail')
        return 'fail'
    vrtwarp_ds = None

    gdal.Unlink('tmp/vrtwarp_10.tif')

    return 'success'

gdaltest_list = [
    vrtwarp_1,
    vrtwarp_2,
    vrtwarp_3,
    vrtwarp_4,
    vrtwarp_5,
    vrtwarp_6,
    vrtwarp_7,
    vrtwarp_8,
    vrtwarp_9,
    vrtwarp_10
     ]


if __name__ == '__main__':

    gdaltest.setup_run( 'vrtwarp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

