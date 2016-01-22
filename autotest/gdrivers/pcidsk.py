#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for PCIDSK driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
from osgeo import ogr

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Read test of floating point reference data.

def pcidsk_1():

    tst = gdaltest.GDALTest( 'PCIDSK', 'utm.pix', 1, 39576 )
    return tst.testOpen()

###############################################################################
# Test lossless copying (16, multiband) via Create().

def pcidsk_2():

    tst = gdaltest.GDALTest( 'PCIDSK', 'rgba16.png', 2, 2042 )

    return tst.testCreate()
    
###############################################################################
# Test copying of georeferencing and projection.

def pcidsk_3():

    tst = gdaltest.GDALTest( 'PCIDSK', 'utm.pix', 1, 39576 )

    return tst.testCreateCopy( check_gt = 1, check_srs = 1 )

###############################################################################
# Test overview reading.

def pcidsk_4():

    ds = gdal.Open( 'data/utm.pix' )

    band = ds.GetRasterBand(1)
    if band.GetOverviewCount() != 1:
        gdaltest.post_reason( 'did not get expected overview count' )
        return 'fail'

    cs = band.GetOverview(0).Checksum()
    if cs != 8368:
        gdaltest.post_reason( 'wrong overview checksum (%d)' % cs )
        return 'fail'

    return 'success'

###############################################################################
# Test writing metadata to a newly created file.

def pcidsk_5():

    # Are we using the new PCIDSK SDK based driver?
    driver = gdal.GetDriverByName( 'PCIDSK' )
    col = driver.GetMetadataItem( 'DMD_CREATIONOPTIONLIST' )
    if col.find('COMPRESSION') == -1:
        gdaltest.pcidsk_new = 0
        return 'skip'
    else:
        gdaltest.pcidsk_new = 1

    # Create testing file.

    gdaltest.pcidsk_ds = driver.Create( 'tmp/pcidsk_5.pix', 400, 600, 1,
                                        gdal.GDT_Byte )

    # Write out some metadata to the default and non-default domain and
    # using the set and single methods.
    
    gdaltest.pcidsk_ds.SetMetadata( [ 'ABC=DEF', 'GHI=JKL' ] )
    gdaltest.pcidsk_ds.SetMetadataItem( 'XXX',  'YYY' )
    gdaltest.pcidsk_ds.SetMetadataItem( 'XYZ',  '123', 'AltDomain' )

    # Close and reopen.
    gdaltest.pcidsk_ds = None
    gdaltest.pcidsk_ds = gdal.Open( 'tmp/pcidsk_5.pix', gdal.GA_Update )

    # Check metadata.
    mddef = gdaltest.pcidsk_ds.GetMetadata()
    if mddef['GHI'] != 'JKL' or mddef['XXX'] != 'YYY':
        print(mddef)
        gdaltest.post_reason( 'file default domain metadata broken. ')

    if gdaltest.pcidsk_ds.GetMetadataItem('GHI') != 'JKL':
        gdaltest.post_reason( 'GetMetadataItem() in default domain metadata broken. ')

    mdalt = gdaltest.pcidsk_ds.GetMetadata('AltDomain')
    if mdalt['XYZ'] != '123':
        print(mdalt)
        gdaltest.post_reason( 'file alt domain metadata broken. ')

    return 'success'

###############################################################################
# Test writing metadata to a band.

def pcidsk_6():

    if gdaltest.pcidsk_new == 0:
        return 'skip'

    # Write out some metadata to the default and non-default domain and
    # using the set and single methods.
    band = gdaltest.pcidsk_ds.GetRasterBand(1)
    
    band.SetMetadata( [ 'ABC=DEF', 'GHI=JKL' ] )
    band.SetMetadataItem( 'XXX',  'YYY' )
    band.SetMetadataItem( 'XYZ',  '123', 'AltDomain' )
    band = None

    # Close and reopen.
    gdaltest.pcidsk_ds = None
    gdaltest.pcidsk_ds = gdal.Open( 'tmp/pcidsk_5.pix', gdal.GA_Update )

    # Check metadata.
    band = gdaltest.pcidsk_ds.GetRasterBand(1)
    mddef = band.GetMetadata()
    if mddef['GHI'] != 'JKL' or mddef['XXX'] != 'YYY':
        print(mddef)
        gdaltest.post_reason( 'channel default domain metadata broken. ')

    mdalt = band.GetMetadata('AltDomain')
    if mdalt['XYZ'] != '123':
        print(mdalt)
        gdaltest.post_reason( 'channel alt domain metadata broken. ')

    return 'success'

###############################################################################
# Test creating a color table and reading it back.

def pcidsk_7():

    if gdaltest.pcidsk_new == 0:
        return 'skip'

    # Write out some metadata to the default and non-default domain and
    # using the set and single methods.
    band = gdaltest.pcidsk_ds.GetRasterBand(1)

    ct = band.GetColorTable()

    if ct is not None:
        gdaltest.post_reason( 'Got color table unexpectedly.' )
        return 'fail'

    ct = gdal.ColorTable()
    ct.SetColorEntry( 0, (0,255,0,255) )
    ct.SetColorEntry( 1, (255,0,255,255) )
    ct.SetColorEntry( 2, (0,0,255,255) )
    band.SetColorTable( ct )

    ct = band.GetColorTable()

    if ct.GetColorEntry(1) != (255,0,255,255):
        gdaltest.post_reason( 'Got wrong color table entry immediately.' )
        return 'fail'
    
    ct = None
    band = None
    
    # Close and reopen.
    gdaltest.pcidsk_ds = None
    gdaltest.pcidsk_ds = gdal.Open( 'tmp/pcidsk_5.pix', gdal.GA_Update )

    band = gdaltest.pcidsk_ds.GetRasterBand(1)
    
    ct = band.GetColorTable()

    if ct.GetColorEntry(1) != (255,0,255,255):
        gdaltest.post_reason( 'Got wrong color table entry after reopen.' )
        return 'fail'

    if band.GetColorInterpretation() != gdal.GCI_PaletteIndex:
        gdaltest.post_reason( 'Not a palette?' )
        return 'fail'

    if band.SetColorTable( None ) != 0:
        gdaltest.post_reason( 'SetColorTable failed.' )
        return 'fail'

    if band.GetColorTable() is not None:
        gdaltest.post_reason( 'color table still exists!' )
        return 'fail'
    
    if band.GetColorInterpretation() != gdal.GCI_Undefined:
        gdaltest.post_reason( 'Paletted?' )
        return 'fail'

    return 'success'

###############################################################################
# Test FILE interleaving.

def pcidsk_8():

    tst = gdaltest.GDALTest( 'PCIDSK', 'rgba16.png', 2, 2042,
                             options = ['INTERLEAVING=FILE'] )

    return tst.testCreate()

###############################################################################
# Test that we cannot open a vector only pcidsk

def pcidsk_9():

    if gdaltest.pcidsk_new == 0:
        return 'skip'

    ogr_drv = ogr.GetDriverByName('PCIDSK')
    if ogr_drv is None:
        return 'skip'

    ds = ogr_drv.CreateDataSource('/vsimem/pcidsk_9.pix')
    ds.CreateLayer('foo')
    ds = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('/vsimem/pcidsk_9.pix')
    gdal.PopErrorHandler()
    if ds is not None:
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/pcidsk_9.pix')

    return 'success'

###############################################################################
# Test overview creation.

def pcidsk_10():
    if gdaltest.pcidsk_new == 0:
        return 'skip'

    src_ds = gdal.Open( 'data/byte.tif' )
    ds = gdal.GetDriverByName('PCIDSK').CreateCopy( '/vsimem/pcidsk_10.pix', src_ds )
    src_ds = None

    #ds = None
    #ds = gdal.Open( '/vsimem/pcidsk_10.pix', gdal.GA_Update )

    band = ds.GetRasterBand(1)
    ds.BuildOverviews('NEAR', [2])

    if band.GetOverviewCount() != 1:
        gdaltest.post_reason( 'did not get expected overview count' )
        return 'fail'

    cs = band.GetOverview(0).Checksum()
    if cs != 1087:
        gdaltest.post_reason( 'wrong overview checksum (%d)' % cs )
        return 'fail'

    ds = None

    gdal.GetDriverByName('PCIDSK').Delete( '/vsimem/pcidsk_10.pix' )

    return 'success'

###############################################################################
# Test INTERLEAVING=TILED interleaving.

def pcidsk_11():
    if gdaltest.pcidsk_new == 0:
        return 'skip'

    tst = gdaltest.GDALTest( 'PCIDSK', 'rgba16.png', 2, 2042,
                             options = ['INTERLEAVING=TILED', 'TILESIZE=32'] )

    return tst.testCreate()

###############################################################################
# Test INTERLEAVING=TILED interleaving and COMPRESSION=RLE

def pcidsk_12():
    if gdaltest.pcidsk_new == 0:
        return 'skip'

    tst = gdaltest.GDALTest( 'PCIDSK', 'rgba16.png', 2, 2042,
                             options = ['INTERLEAVING=TILED', 'TILESIZE=32', 'COMPRESSION=RLE'] )

    return tst.testCreate()

###############################################################################
# Test INTERLEAVING=TILED interleaving and COMPRESSION=JPEG

def pcidsk_13():
    if gdaltest.pcidsk_new == 0:
        return 'skip'

    if gdal.GetDriverByName('JPEG') is None:
        return 'skip'

    src_ds = gdal.Open( 'data/byte.tif' )
    ds = gdal.GetDriverByName('PCIDSK').CreateCopy( '/vsimem/pcidsk_13.pix', src_ds, options = ['INTERLEAVING=TILED', 'COMPRESSION=JPEG'] )
    src_ds = None

    gdal.Unlink( '/vsimem/pcidsk_13.pix.aux.xml' )

    ds = None
    ds = gdal.Open( '/vsimem/pcidsk_13.pix' )
    band = ds.GetRasterBand(1)
    desc = band.GetDescription()
    cs = band.Checksum()
    ds = None

    gdal.GetDriverByName('PCIDSK').Delete( '/vsimem/pcidsk_13.pix' )

    if cs != 4645:
        gdaltest.post_reason('bad checksum')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test SetDescription()

def pcidsk_14():
    if gdaltest.pcidsk_new == 0:
        return 'skip'

    ds = gdal.GetDriverByName('PCIDSK').Create( '/vsimem/pcidsk_14.pix', 1, 1 )
    band = ds.GetRasterBand(1).SetDescription('mydescription')
    src_ds = None

    gdal.Unlink( '/vsimem/pcidsk_14.pix.aux.xml' )

    ds = None
    ds = gdal.Open( '/vsimem/pcidsk_14.pix' )
    band = ds.GetRasterBand(1)
    desc = band.GetDescription()
    ds = None

    gdal.GetDriverByName('PCIDSK').Delete( '/vsimem/pcidsk_14.pix' )

    if desc != 'mydescription':
        gdaltest.post_reason('bad description')
        print(desc)
        return 'fail'

    return 'success'

###############################################################################
# Check various items from a modern irvine.pix

def pcidsk_online_1():
    if gdaltest.pcidsk_new == 0:
        return 'skip'
    
    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/pcidsk/sdk_testsuite/irvine_gcp2.pix', 'irvine_gcp2.pix'):
        return 'skip'

    ds = gdal.Open('tmp/cache/irvine_gcp2.pix')

    band = ds.GetRasterBand(6)

    names = band.GetRasterCategoryNames()

    exp_names = ['', '', '', '', '', '', '', '', '', '', '', 'Residential', 'Commercial', 'Industrial', 'Transportation', 'Commercial/Industrial', 'Mixed', 'Other', '', '', '', 'Crop/Pasture', 'Orchards', 'Feeding', 'Other', '', '', '', '', '', '', 'Herbaceous', 'Shrub', 'Mixed', '', '', '', '', '', '', '', 'Deciduous', 'Evergreen', 'Mixed', '', '', '', '', '', '', '', 'Streams/Canals', 'Lakes', 'Reservoirs', 'Bays/Estuaries', '', '', '', '', '', '', 'Forested', 'Nonforested', '', '', '', '', '', '', '', '', 'Dry_Salt_Flats', 'Beaches', 'Sandy_Areas', 'Exposed_Rock', 'Mines/Quarries/Pits', 'Transitional_Area', 'Mixed', '', '', '', 'Shrub/Brush', 'Herbaceous', 'Bare', 'Wet', 'Mixed', '', '', '', '', '', 'Perennial_Snow', 'Glaciers']

    if names != exp_names:
        print(names)
        gdaltest.post_reason( 'did not get expected category names.' )
        return 'false'

    band = ds.GetRasterBand(20)
    if band.GetDescription() != 'Training site for type 2 crop':
        gdaltest.post_reason( 'did not get expected band 20 description' )
        return 'fail'

    exp_checksum = 2057
    checksum = band.Checksum()
    if exp_checksum != checksum:
        print(checksum)
        gdaltest.post_reason( 'did not get right bitmap checksum.')
        return 'fail'

    md = band.GetMetadata('IMAGE_STRUCTURE')
    if md['NBITS'] != '1':
        gdaltest.post_reason( 'did not get expected NBITS=1 metadata.' )
        return 'fail'

    return 'success'

###############################################################################
# Cleanup.

def pcidsk_cleanup():
    gdaltest.pcidsk_ds = None
    gdaltest.clean_tmp()
    return 'success'

gdaltest_list = [
    pcidsk_1,
    pcidsk_2,
    pcidsk_3,
    pcidsk_4,
    pcidsk_5,
    pcidsk_6,
    pcidsk_7,
    pcidsk_8,
    pcidsk_9,
    pcidsk_10,
    pcidsk_11,
    pcidsk_12,
    pcidsk_13,
    pcidsk_14,
    pcidsk_online_1,
    pcidsk_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'pcidsk' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

