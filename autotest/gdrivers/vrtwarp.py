#!/usr/bin/env pytest
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
import shutil
import sys
from osgeo import gdal


import gdaltest
import pytest

###############################################################################
# Verify reading from simple existing warp definition.


def test_vrtwarp_1():

    tst = gdaltest.GDALTest('VRT', 'vrt/rgb_warp.vrt', 2, 21504)
    return tst.testOpen(check_filelist=False)

###############################################################################
# Create a new VRT warp in the temp directory.


def test_vrtwarp_2():

    try:
        os.remove('tmp/warp.vrt')
    except OSError:
        pass

    gcp_ds = gdal.OpenShared('data/rgb_gcp.vrt', gdal.GA_ReadOnly)

    gdaltest.vrtwarp_ds = gdal.AutoCreateWarpedVRT(gcp_ds)

    gcp_ds = None

    checksum = gdaltest.vrtwarp_ds.GetRasterBand(2).Checksum()
    expected = 21504
    assert checksum == expected, ('Got checksum of %d instead of expected %d.'
                             % (checksum, expected))

###############################################################################
# Force the VRT warp file to be written to disk and close it.  Reopen, and
# verify checksum.


def test_vrtwarp_3():

    gdaltest.vrtwarp_ds.SetDescription('tmp/warp.vrt')
    gdaltest.vrtwarp_ds = None

    gdaltest.vrtwarp_ds = gdal.Open('tmp/warp.vrt', gdal.GA_ReadOnly)

    checksum = gdaltest.vrtwarp_ds.GetRasterBand(2).Checksum()
    expected = 21504

    gdaltest.vrtwarp_ds = None
    gdal.GetDriverByName('VRT').Delete('tmp/warp.vrt')

    assert checksum == expected, ('Got checksum of %d instead of expected %d.'
                             % (checksum, expected))

###############################################################################
# Test implicit overviews with default source overview level strategy (AUTO)


def test_vrtwarp_4():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vrtwarp_4.tif', src_ds)
    cs_main = tmp_ds.GetRasterBand(1).Checksum()
    tmp_ds.BuildOverviews('NONE', overviewlist=[2, 4])
    tmp_ds.GetRasterBand(1).GetOverview(0).Fill(127)
    cs_ov0 = tmp_ds.GetRasterBand(1).GetOverview(0).Checksum()
    tmp_ds.GetRasterBand(1).GetOverview(1).Fill(255)
    cs_ov1 = tmp_ds.GetRasterBand(1).GetOverview(1).Checksum()

    vrtwarp_ds = gdal.AutoCreateWarpedVRT(tmp_ds)
    tmp_ds = None

    for i in range(3):
        assert vrtwarp_ds.GetRasterBand(1).GetOverviewCount() == 2
        assert vrtwarp_ds.GetRasterBand(1).Checksum() == cs_main, i
        assert vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() == cs_ov0
        assert vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() == cs_ov1
        if i == 0:
            vrtwarp_ds.SetDescription('tmp/vrtwarp_4.vrt')
            vrtwarp_ds = None
            vrtwarp_ds = gdal.Open('tmp/vrtwarp_4.vrt')
        elif i == 1:
            vrtwarp_ds = None
            tmp_ds = gdal.Open('tmp/vrtwarp_4.tif')
            vrtwarp_ds = gdal.AutoCreateWarpedVRT(tmp_ds)
            vrtwarp_ds.SetMetadataItem('SrcOvrLevel', 'AUTO')
            vrtwarp_ds.SetDescription('tmp/vrtwarp_4.vrt')
            tmp_ds = None

    # Add an explicit overview
    vrtwarp_ds.BuildOverviews('NEAR', overviewlist=[2, 4, 8])
    vrtwarp_ds = None

    ds = gdal.GetDriverByName('MEM').Create('', 3, 3, 1)
    ds.GetRasterBand(1).Fill(255)
    expected_cs_ov2 = ds.GetRasterBand(1).Checksum()
    ds = None

    vrtwarp_ds = gdal.Open('tmp/vrtwarp_4.vrt')
    assert vrtwarp_ds.GetRasterBand(1).GetOverviewCount() == 3
    assert vrtwarp_ds.GetRasterBand(1).Checksum() == cs_main
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() == cs_ov0
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() == cs_ov1
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(2).Checksum() == expected_cs_ov2
    vrtwarp_ds = None

    gdal.Unlink('tmp/vrtwarp_4.vrt')
    gdal.Unlink('tmp/vrtwarp_4.tif')

###############################################################################
# Test implicit overviews with selection of the upper source overview level


def test_vrtwarp_5():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vrtwarp_5.tif', src_ds)
    cs_main = tmp_ds.GetRasterBand(1).Checksum()
    tmp_ds.BuildOverviews('NONE', overviewlist=[2, 4])
    tmp_ds.GetRasterBand(1).GetOverview(0).Fill(127)
    tmp_ds.GetRasterBand(1).GetOverview(0).Checksum()
    tmp_ds.GetRasterBand(1).GetOverview(1).Fill(255)
    tmp_ds.GetRasterBand(1).GetOverview(1).Checksum()
    tmp_ds = None

    ds = gdal.Warp('', 'tmp/vrtwarp_5.tif', options='-of MEM -ovr NONE -overwrite -ts 10 10')
    expected_cs_ov0 = ds.GetRasterBand(1).Checksum()
    ds = None

    ds = gdal.GetDriverByName('MEM').Create('', 5, 5, 1)
    ds.GetRasterBand(1).Fill(127)
    expected_cs_ov1 = ds.GetRasterBand(1).Checksum()
    ds = None

    tmp_ds = gdal.Open('tmp/vrtwarp_5.tif')
    vrtwarp_ds = gdal.AutoCreateWarpedVRT(tmp_ds)
    vrtwarp_ds.SetMetadataItem('SrcOvrLevel', 'AUTO-1')
    tmp_ds = None
    assert vrtwarp_ds.GetRasterBand(1).GetOverviewCount() == 2
    assert vrtwarp_ds.GetRasterBand(1).Checksum() == cs_main
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() == expected_cs_ov0
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() == expected_cs_ov1
    vrtwarp_ds = None

    gdal.Unlink('tmp/vrtwarp_5.tif')

###############################################################################
# Test implicit overviews with GCP


def test_vrtwarp_6():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vrtwarp_6.tif', src_ds)
    cs_main = tmp_ds.GetRasterBand(1).Checksum()
    tmp_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])  # cancel geotransform
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
    tmp_ds.BuildOverviews('NEAR', overviewlist=[2, 4])
    cs_ov0 = tmp_ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs_ov1 = tmp_ds.GetRasterBand(1).GetOverview(1).Checksum()

    vrtwarp_ds = gdal.AutoCreateWarpedVRT(tmp_ds)
    vrtwarp_ds.SetDescription('tmp/vrtwarp_6.vrt')
    vrtwarp_ds = None
    tmp_ds = None

    vrtwarp_ds = gdal.Open('tmp/vrtwarp_6.vrt')

    assert vrtwarp_ds.GetRasterBand(1).GetOverviewCount() == 2
    assert vrtwarp_ds.GetRasterBand(1).Checksum() == cs_main
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() == cs_ov0
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() == cs_ov1

    gdal.Unlink('tmp/vrtwarp_6.vrt')
    gdal.Unlink('tmp/vrtwarp_6.tif')

###############################################################################
# Test implicit overviews with GCP (TPS)


def test_vrtwarp_7():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vrtwarp_7.tif', src_ds)
    cs_main = tmp_ds.GetRasterBand(1).Checksum()
    tmp_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])  # cancel geotransform
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
    tmp_ds.BuildOverviews('NEAR', overviewlist=[2, 4])
    cs_ov0 = tmp_ds.GetRasterBand(1).GetOverview(0).Checksum()
    cs_ov1 = tmp_ds.GetRasterBand(1).GetOverview(1).Checksum()
    tmp_ds = None

    vrtwarp_ds = gdal.Warp('tmp/vrtwarp_7.vrt', 'tmp/vrtwarp_7.tif', options='-overwrite -of VRT -tps')
    assert vrtwarp_ds.GetRasterBand(1).GetOverviewCount() == 2
    assert vrtwarp_ds.GetRasterBand(1).Checksum() == cs_main
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() == cs_ov0
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() == cs_ov1
    vrtwarp_ds = None

    gdal.Unlink('tmp/vrtwarp_7.vrt')
    gdal.Unlink('tmp/vrtwarp_7.tif')

###############################################################################
# Test implicit overviews with RPC


def test_vrtwarp_8():

    shutil.copy('../gcore/data/byte.tif', 'tmp/vrtwarp_8.tif')
    shutil.copy('../gcore/data/test_rpc.txt', 'tmp/vrtwarp_8_rpc.txt')
    ds = gdal.Open('tmp/vrtwarp_8.tif', gdal.GA_Update)
    ds.BuildOverviews('NEAR', overviewlist=[2])
    ds = None

    ds = gdal.Warp('', 'tmp/vrtwarp_8.tif', options='-of MEM -rpc')
    expected_cs_main = ds.GetRasterBand(1).Checksum()
    ds = None

    vrtwarp_ds = gdal.Warp('tmp/vrtwarp_8.vrt', 'tmp/vrtwarp_8.tif', options='-overwrite -of VRT -rpc')
    assert vrtwarp_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert vrtwarp_ds.GetRasterBand(1).Checksum() == expected_cs_main
    if vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() != 1214:
        print(vrtwarp_ds.GetRasterBand(1).GetOverview(0).XSize)
        pytest.fail(vrtwarp_ds.GetRasterBand(1).GetOverview(0).YSize)
    vrtwarp_ds = None

    gdal.Unlink('tmp/vrtwarp_8.vrt')
    gdal.Unlink('tmp/vrtwarp_8.tif')
    gdal.Unlink('tmp/vrtwarp_8_rpc.txt')

###############################################################################
# Test implicit overviews with GEOLOCATION


def test_vrtwarp_9():

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
    ds.BuildOverviews('NEAR', overviewlist=[2])
    ds = None

    ds = gdal.Warp('', 'tmp/sstgeo.vrt', options='-of MEM -geoloc')
    expected_cs_main = ds.GetRasterBand(1).Checksum()
    ds = None

    vrtwarp_ds = gdal.Warp('tmp/vrtwarp_9.vrt', 'tmp/sstgeo.vrt', options='-overwrite -of VRT -geoloc')
    assert vrtwarp_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert vrtwarp_ds.GetRasterBand(1).Checksum() == expected_cs_main
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() == 63696, \
        (vrtwarp_ds.GetRasterBand(1).GetOverview(0).XSize, vrtwarp_ds.GetRasterBand(1).GetOverview(0).YSize)
    vrtwarp_ds = None

    gdal.Unlink('tmp/vrtwarp_9.vrt')
    gdal.Unlink('tmp/sstgeo.vrt')
    gdal.Unlink('tmp/sstgeo.vrt.ovr')
    gdal.Unlink('tmp/sstgeo.tif')

###############################################################################
# Test implicit overviews with selection of the full resolution level


def test_vrtwarp_10():

    src_ds = gdal.Open('../gcore/data/byte.tif')
    tmp_ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vrtwarp_10.tif', src_ds)
    cs_main = tmp_ds.GetRasterBand(1).Checksum()
    tmp_ds.BuildOverviews('NONE', overviewlist=[2, 4])
    tmp_ds.GetRasterBand(1).GetOverview(0).Fill(127)
    tmp_ds.GetRasterBand(1).GetOverview(0).Checksum()
    tmp_ds.GetRasterBand(1).GetOverview(1).Fill(255)
    tmp_ds.GetRasterBand(1).GetOverview(1).Checksum()
    tmp_ds = None

    ds = gdal.Warp('', 'tmp/vrtwarp_10.tif', options='-of MEM -ovr NONE -ts 10 10')
    expected_cs_ov0 = ds.GetRasterBand(1).Checksum()
    ds = None

    ds = gdal.Warp('', 'tmp/vrtwarp_10.tif', options='-of MEM -ovr NONE -ts 5 5')
    expected_cs_ov1 = ds.GetRasterBand(1).Checksum()
    ds = None

    tmp_ds = gdal.Open('tmp/vrtwarp_10.tif')
    vrtwarp_ds = gdal.AutoCreateWarpedVRT(tmp_ds)
    vrtwarp_ds.SetMetadataItem('SrcOvrLevel', 'NONE')
    tmp_ds = None
    assert vrtwarp_ds.GetRasterBand(1).GetOverviewCount() == 2
    assert vrtwarp_ds.GetRasterBand(1).Checksum() == cs_main
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(0).Checksum() == expected_cs_ov0
    assert vrtwarp_ds.GetRasterBand(1).GetOverview(1).Checksum() == expected_cs_ov1
    vrtwarp_ds = None

    gdal.Unlink('tmp/vrtwarp_10.tif')

###############################################################################
# Test implicit overviews with dest alpha band (#6081)


def test_vrtwarp_11():

    ds = gdal.Open('data/vrt/bug6581.vrt')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    ds = None

    assert cs1 == 22122 and cs2 == 56685 and cs3 == 22122

###############################################################################
# Test reading a regular VRT whose source is a warped VRT inlined


def test_vrtwarp_read_vrt_of_warped_vrt():

    ds = gdal.Open('data/vrt/vrt_of_warped_vrt.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4672

###############################################################################
# Test reading a warped VRT with blocks > 2 gigapixels


def test_vrtwarp_read_blocks_larger_than_2_gigapixels():

    if not gdaltest.run_slow_tests():
        pytest.skip()
    if sys.maxsize < 2**32:
        pytest.skip('Test not available on 32 bit')

    import psutil
    if psutil.virtual_memory().available < 2 * 50000 * 50000:
        pytest.skip("Not enough virtual memory available")

    ds = gdal.Open('data/vrt/test_deflate_2GB.vrt')

    data  = ds.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, buf_xsize = 20, buf_ysize = 20)
    assert data
    ref_ds = gdal.GetDriverByName('MEM').Create('', 20, 20)
    ref_ds.GetRasterBand(1).Fill(127)
    assert data == ref_ds.ReadRaster()

###############################################################################
# Test reading a warped VRT that has blocks pointing to space.
# https://github.com/OSGeo/gdal/issues/1985


def test_vrtwarp_read_blocks_in_space():

    ds = gdal.Open('data/vrt/geos_vrtwarp.vrt')
    assert ds.GetRasterBand(1).ReadRaster(0, 0, 512, 512)

###############################################################################
# Test reading a warped VRT that has inconsistent block size at band and
# dataset level


@pytest.mark.parametrize("filename", ["data/vrt/warp_inconsistent_blockxsize.vrt",
                                      "data/vrt/warp_inconsistent_blockysize.vrt"])
def test_vrtwarp_read_inconsistent_blocksize(filename):

    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = gdal.Open(filename)
    assert ds is None
    assert gdal.GetLastErrorMsg() == 'Block size specified on band 1 not consistent with dataset block size'


###############################################################################
# Test that we don't write duplicated block size information


def test_vrtwarp_write_no_duplicated_blocksize():
    tmpfilename = '/vsimem/tmp.vrt'
    gdal.Warp(tmpfilename, 'data/byte.tif', format='VRT', width=1024, height=1024)
    fp = gdal.VSIFOpenL(tmpfilename, 'rb')
    assert fp
    data = gdal.VSIFReadL(1, 10000, fp).decode('utf-8')
    gdal.VSIFCloseL(fp)
    gdal.Unlink(tmpfilename)
    assert '<BlockXSize>' in data
    assert '<BlockYSize>' in data
    assert ' blockXSize=' not in data
    assert ' blockYSize=' not in data
