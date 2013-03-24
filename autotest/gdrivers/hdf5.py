#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for HDF5 driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at mines dash paris dot org>
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
import shutil

try:
    from osgeo import gdal
except ImportError:
    import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test if HDF5 driver is present

def hdf5_1():

    try:
        gdaltest.hdf5_drv = gdal.GetDriverByName( 'HDF5' )
    except:
        gdaltest.hdf5_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Confirm expected subdataset information.

def hdf5_2():

    if gdaltest.hdf5_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/groups.h5' )

    sds_list = ds.GetMetadata('SUBDATASETS')

    if len(sds_list) != 4:
        print(sds_list)
        gdaltest.post_reason( 'Did not get expected subdataset count.' )
        return 'fail'

    if sds_list['SUBDATASET_1_NAME'] != 'HDF5:"data/groups.h5"://MyGroup/Group_A/dset2' \
       or sds_list['SUBDATASET_2_NAME'] != 'HDF5:"data/groups.h5"://MyGroup/dset1':
        print(sds_list)
        gdaltest.post_reason( 'did not get expected subdatasets.' )
        return 'fail'

    return 'success'

###############################################################################
# Confirm that single variable files can be accessed directly without
# subdataset stuff.

def hdf5_3():

    if gdaltest.hdf5_drv is None:
        return 'skip'

    ds = gdal.Open( 'HDF5:"data/u8be.h5"://TestArray' )

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 135:
        gdaltest.post_reason( 'did not get expected checksum' )
        return 'fail'

    return 'success'

###############################################################################
# Confirm subdataset access, and checksum.

def hdf5_4():

    if gdaltest.hdf5_drv is None:
        return 'skip'

    ds = gdal.Open( 'HDF5:"data/u8be.h5"://TestArray' )

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 135:
        gdaltest.post_reason( 'did not get expected checksum' )
        return 'fail'

    return 'success'

###############################################################################
# Similar check on a 16bit dataset.

def hdf5_5():

    if gdaltest.hdf5_drv is None:
        return 'skip'

    ds = gdal.Open( 'HDF5:"data/groups.h5"://MyGroup/dset1' )

    cs = ds.GetRasterBand(1).Checksum()
    if cs != 18:
        gdaltest.post_reason( 'did not get expected checksum' )
        return 'fail'

    return 'success'

###############################################################################
# Test generating an overview on a subdataset.

def hdf5_6():

    if gdaltest.hdf5_drv is None:
        return 'skip'

    shutil.copyfile( 'data/groups.h5', 'tmp/groups.h5' )

    ds = gdal.Open( 'HDF5:"tmp/groups.h5"://MyGroup/dset1' )
    ds.BuildOverviews( overviewlist = [2] )
    ds = None

    ds = gdal.Open( 'HDF5:"tmp/groups.h5"://MyGroup/dset1' )
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason( 'failed to find overview' )
        return 'fail'
    ds = None

    # confirm that it works with a different path. (#3290)

    ds = gdal.Open( 'HDF5:"data/../tmp/groups.h5"://MyGroup/dset1' )
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason( 'failed to find overview with alternate path' )
        return 'fail'
    ovfile = ds.GetMetadataItem('OVERVIEW_FILE','OVERVIEWS')
    if ovfile[:11] != 'data/../tmp':
        print(ovfile)
        gdaltest.post_reason( 'did not get expected OVERVIEW_FILE.' )
        return 'fail'
    ds = None


    gdaltest.clean_tmp()

    return 'success'

###############################################################################
# Coarse metadata check (regression test for #2412).

def hdf5_7():

    if gdaltest.hdf5_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/metadata.h5' )
    metadata = ds.GetMetadata()
    metadataList = ds.GetMetadata_List()
    ds = None

    if len(metadata) != len(metadataList):
        gdaltest.post_reason( 'error in metadata dictionary setup' )
        return 'fail'

    metadataList = [item.split('=', 1)[0] for item in metadataList]
    for key in metadataList:
        try:
            metadata.pop(key)
        except KeyError:
            gdaltest.post_reason( 'unable to fing "%s" key' % key )
            return 'fail'
    return 'success'

###############################################################################
# Test metadata names.

def hdf5_8():

    if gdaltest.hdf5_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/metadata.h5' )
    metadata = ds.GetMetadata()
    ds = None

    if len(metadata) == 0:
        gdaltest.post_reason( 'no metadata found' )
        return 'fail'

    h5groups = ['G1', 'Group with spaces', 'Group_with_underscores',
                'Group with spaces_and_underscores']
    h5datasets = ['D1', 'Dataset with spaces', 'Dataset_with_underscores',
                  'Dataset with spaces_and_underscores']
    attributes = {
        'attribute': 'value',
        'attribute with spaces': 0,
        'attribute_with underscores': 0,
        'attribute with spaces_and_underscores': .1,
    }

    def scanMetadata(parts):
        for attr in attributes:
            name = '_'.join(parts + [attr])
            name = name.replace(' ', '_')
            if name not in metadata:
                gdaltest.post_reason( 'unable to find metadata: "%s"' % name )
                return 'fail'

            value = metadata.pop(name)

            value = value.strip(' d')
            value = type(attributes[attr])(value)
            if value != attributes[attr]:
                gdaltest.post_reason( 'incorrect metadata value for "%s": '
                                       '"%s" != "%s"' % (name, value,
                                                         attributes[attr]) )
                return 'fail'

    # level0
    if scanMetadata([]) is not None:
        return 'fail'

    # level1 datasets
    for h5dataset in h5datasets:
        if scanMetadata([h5dataset]) is not None:
            return 'fail'

    # level1 groups
    for h5group in h5groups:
        if scanMetadata([h5group]) is not None:
            return 'fail'

        # level2 datasets
        for h5dataset in h5datasets:
            if scanMetadata([h5group, h5dataset]) is not None:
                return 'fail'


    return 'success'

###############################################################################
# Variable length string metadata check (regression test for #4228).

def hdf5_9():

    if gdaltest.hdf5_drv is None:
        return 'skip'

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        gdaltest.post_reason('would crash')
        return 'skip'

    ds = gdal.Open( 'data/vlstr_metadata.h5' )
    metadata = ds.GetRasterBand(1).GetMetadata()
    ds = None

    ref_metadata = {
        'TEST_BANDNAMES': 'SAA',
        'TEST_CODING': '0.6666666667 0.0000000000 TRUE',
        'TEST_FLAGS': '255=noValue',
        'TEST_MAPPING': 'Geographic Lat/Lon 0.5000000000 0.5000000000 27.3154761905 -5.0833333333 0.0029761905 0.0029761905 WGS84 Degrees',
        'TEST_NOVALUE': '255',
        'TEST_RANGE': '0 255 0 255',
    }

    if len(metadata) != len(ref_metadata):
        gdaltest.post_reason( 'incorrect number of metadata: '
                              'expected %d, got %d' % (len(ref_metadata),
                                                       len(metadata)) )
        return 'fail'

    for key in metadata:
        if key not in ref_metadata:
            gdaltest.post_reason( 'unexpected metadata key "%s"' % key )
            return 'fail'

        if metadata[key] != ref_metadata[key]:
            gdaltest.post_reason( 'incorrect metadata value for key "%s": '
                                  'expected "%s", got "%s" '%
                                    (key, ref_metadata[key], metadata[key]) )
            return 'fail'

    return 'success'

###############################################################################
# Test CSK_DGM.h5 (#4160)

def hdf5_10():

    if gdaltest.hdf5_drv is None:
        return 'skip'

    # Try opening the QLK subdataset to check that no error is generated
    gdal.ErrorReset()
    ds = gdal.Open( 'HDF5:"data/CSK_DGM.h5"://S01/QLK' )
    if ds is None or gdal.GetLastErrorMsg() != '':
           gdaltest.post_reason('fail')
           return 'fail'
    ds = None

    ds = gdal.Open( 'HDF5:"data/CSK_DGM.h5"://S01/SBI' )
    got_gcpprojection = ds.GetGCPProjection()
    expected_gcpprojection = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]]'
    if got_gcpprojection != expected_gcpprojection:
        print(got_gcpprojection)
        gdaltest.post_reason('fail')
        return 'fail'

    got_gcps = ds.GetGCPs()
    if len(got_gcps) != 4:
           gdaltest.post_reason('fail')
           return 'fail'

    if abs(got_gcps[0].GCPPixel - 0) > 1e-5 or abs(got_gcps[0].GCPLine - 0) > 1e-5 or \
       abs(got_gcps[0].GCPX - 44.7280047434954) > 1e-5 or abs(got_gcps[0].GCPY - 12.2395902509238) > 1e-5:
           gdaltest.post_reason('fail')
           return 'fail'

    return 'success'

###############################################################################
# Test CSK_GEC.h5 (#4160)

def hdf5_11():

    if gdaltest.hdf5_drv is None:
        return 'skip'

    # Try opening the QLK subdataset to check that no error is generated
    gdal.ErrorReset()
    ds = gdal.Open( 'HDF5:"data/CSK_GEC.h5"://S01/QLK' )
    if ds is None or gdal.GetLastErrorMsg() != '':
           gdaltest.post_reason('fail')
           return 'fail'
    ds = None

    ds = gdal.Open( 'HDF5:"data/CSK_GEC.h5"://S01/SBI' )
    got_projection = ds.GetProjection()
    expected_projection = 'PROJCS["unnamed",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",15],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0]]'
    if got_projection != expected_projection:
        print(got_projection)
        gdaltest.post_reason('fail')
        return 'fail'

    got_gt = ds.GetGeoTransform()
    expected_gt = (275592.5, 2.5, 0.0, 4998152.5, 0.0, -2.5)
    for i in range(6):
        if abs(got_gt[i] - expected_gt[i]) > 1e-5:
            print(got_gt)
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'

###############################################################################
#
class TestHDF5:
    def __init__( self, downloadURL, fileName, subdatasetname, checksum, download_size ):
        self.downloadURL = downloadURL
        self.fileName = fileName
        self.subdatasetname = subdatasetname
        self.checksum = checksum
        self.download_size = download_size

    def test( self ):
        if gdaltest.hdf5_drv is None:
            return 'skip'

        if not gdaltest.download_file(self.downloadURL + '/' + self.fileName, self.fileName, self.download_size):
            return 'skip'

        ds = gdal.Open('HDF5:"tmp/cache/' + self.fileName + '"://' +  self.subdatasetname)

        if ds.GetRasterBand(1).Checksum() != self.checksum:
            gdaltest.post_reason('Bad checksum. Expected %d, got %d' % (self.checksum, ds.GetRasterBand(1).Checksum()))
            return 'failure'

        return 'success'



gdaltest_list = [
    hdf5_1,
    hdf5_2,
    hdf5_3,
    hdf5_4,
    hdf5_5,
    hdf5_6,
    hdf5_7,
    hdf5_8,
    hdf5_9,
    hdf5_10,
    hdf5_11
]

hdf5_list = [ ('ftp://ftp.hdfgroup.uiuc.edu/pub/outgoing/hdf_files/hdf5/samples/convert', 'C1979091.h5',
                                     'HDF4_PALGROUP/HDF4_PALETTE_2', 7488, -1),
              ('ftp://ftp.hdfgroup.uiuc.edu/pub/outgoing/hdf_files/hdf5/samples/convert', 'C1979091.h5',
                                     'Raster_Image_#0', 3661, -1),
              ('ftp://ftp.hdfgroup.uiuc.edu/pub/outgoing/hdf_files/hdf5/geospatial/DEM', 'half_moon_bay.grid',
                                     'HDFEOS/GRIDS/DEMGRID/Data_Fields/Elevation', 30863, -1),
            ]

for item in hdf5_list:
    ut = TestHDF5( item[0], item[1], item[2], item[3], item[4] )
    gdaltest_list.append( (ut.test, 'HDF5:"' + item[1]+ '"://' +   item[2]) )


if __name__ == '__main__':

    gdaltest.setup_run( 'hdf5' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

