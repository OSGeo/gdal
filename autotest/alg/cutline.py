#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test the image reprojection functions. Try to test as many
#           resamplers as possible (we have optimized resamplers for some
#           data types, test them too).
# Author:   Andrey Kiselev, dron16@ak4719.spb.edu
#
###############################################################################
# Copyright (c) 2008, Andrey Kiselev <dron16@ak4719.spb.edu>
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

from osgeo import gdal
import ogrtest
import gdaltest

###############################################################################

def cutline_1():

    tst = gdaltest.GDALTest( 'VRT', 'cutline_noblend.vrt', 1, 11409 )
    return tst.testOpen()

###############################################################################

def cutline_2():

    if not ogrtest.have_geos():
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'cutline_blend.vrt', 1, 21395 )
    return tst.testOpen()

###############################################################################

def cutline_3():

    if not ogrtest.have_geos():
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'cutline_multipolygon.vrt', 1, 20827 )
    return tst.testOpen()

###############################################################################

def cutline_4():

    if not ogrtest.have_geos():
        return 'skip'

    ds = gdal.Translate('/vsimem/utmsmall.tif', '../gcore/data/utmsmall.tif')
    ds.BuildOverviews('NEAR', [2])
    ds = None

    ds = gdal.Open("""<VRTDataset rasterXSize="100" rasterYSize="100" subClass="VRTWarpedDataset">
  <SRS>PROJCS[&quot;NAD27 / UTM zone 11N&quot;,GEOGCS[&quot;NAD27&quot;,DATUM[&quot;North_American_Datum_1927&quot;,SPHEROID[&quot;Clarke 1866&quot;,6378206.4,294.9786982139006,AUTHORITY[&quot;EPSG&quot;,&quot;7008&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;6267&quot;]],PRIMEM[&quot;Greenwich&quot;,0],UNIT[&quot;degree&quot;,0.0174532925199433],AUTHORITY[&quot;EPSG&quot;,&quot;4267&quot;]],PROJECTION[&quot;Transverse_Mercator&quot;],PARAMETER[&quot;latitude_of_origin&quot;,0],PARAMETER[&quot;central_meridian&quot;,-117],PARAMETER[&quot;scale_factor&quot;,0.9996],PARAMETER[&quot;false_easting&quot;,500000],PARAMETER[&quot;false_northing&quot;,0],UNIT[&quot;metre&quot;,1,AUTHORITY[&quot;EPSG&quot;,&quot;9001&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;26711&quot;]]</SRS>
  <GeoTransform>  4.4072000000000000e+05,  5.9999999999999993e+01,  0.0000000000000000e+00,  3.7513200000000000e+06,  0.0000000000000000e+00, -5.9999999999999993e+01</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1" subClass="VRTWarpedRasterBand"/>
  <BlockXSize>512</BlockXSize>
  <BlockYSize>128</BlockYSize>
  <GDALWarpOptions>
    <WarpMemoryLimit>6.71089e+07</WarpMemoryLimit>
    <ResampleAlg>NearestNeighbour</ResampleAlg>
    <WorkingDataType>Byte</WorkingDataType>
    <Option name="INIT_DEST">0</Option>
    <SourceDataset>/vsimem/utmsmall.tif</SourceDataset>
    <Transformer>
      <ApproxTransformer>
        <MaxError>0.125</MaxError>
        <BaseTransformer>
          <GenImgProjTransformer>
            <SrcGeoTransform>440720,60,0,3751320,0,-60</SrcGeoTransform>
            <SrcInvGeoTransform>-7345.333333333333,0.01666666666666667,0,62522,0,-0.01666666666666667</SrcInvGeoTransform>
            <DstGeoTransform>440720,59.99999999999999,0,3751320,0,-59.99999999999999</DstGeoTransform>
            <DstInvGeoTransform>-7345.333333333334,0.01666666666666667,0,62522.00000000001,0,-0.01666666666666667</DstInvGeoTransform>
          </GenImgProjTransformer>
        </BaseTransformer>
      </ApproxTransformer>
    </Transformer>
    <BandList>
      <BandMapping src="1" dst="1"/>
    </BandList>
    <Cutline>MULTIPOLYGON(((10 10,10 50,60 50, 10 10)),((70 70,70 100,100 100,100 70,70 70),(80 80,80 90,90 90,90 80,80 80)))</Cutline>
  </GDALWarpOptions>
</VRTDataset>""")
    out_ds = gdal.Translate('', ds, options = '-of MEM -outsize 50%% 50%%')
    cs = out_ds.GetRasterBand(1).Checksum()
    if cs != 5170:
        print(cs)
        return 'fail'

    gdal.Unlink('/vsimem/utmsmall.tif')

    return 'success'

###############################################################################

gdaltest_list = [
    cutline_1,
    cutline_2,
    cutline_3,
    cutline_4
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'cutline' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

