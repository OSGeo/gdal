#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for PCIDSK driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal
from osgeo import ogr


import gdaltest
import pytest

###############################################################################
# Read test of floating point reference data.


def test_pcidsk_1():

    tst = gdaltest.GDALTest('PCIDSK', 'pcidsk/utm.pix', 1, 39576)
    return tst.testOpen()

###############################################################################
# Test lossless copying (16, multiband) via Create().


def test_pcidsk_2():

    tst = gdaltest.GDALTest('PCIDSK', 'png/rgba16.png', 2, 2042)

    return tst.testCreate()

###############################################################################
# Test copying of georeferencing and projection.


def test_pcidsk_3():

    tst = gdaltest.GDALTest('PCIDSK', 'pcidsk/utm.pix', 1, 39576)

    return tst.testCreateCopy(check_gt=1, check_srs=1)

###############################################################################
# Test overview reading.


def test_pcidsk_4():

    ds = gdal.Open('data/pcidsk/utm.pix')

    band = ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 1, 'did not get expected overview count'

    cs = band.GetOverview(0).Checksum()
    assert cs == 8368, ('wrong overview checksum (%d)' % cs)

###############################################################################
# Test writing metadata to a newly created file.


def test_pcidsk_5():

    # Are we using the new PCIDSK SDK based driver?
    driver = gdal.GetDriverByName('PCIDSK')
    col = driver.GetMetadataItem('DMD_CREATIONOPTIONLIST')
    if col.find('COMPRESSION') == -1:
        gdaltest.pcidsk_new = 0
        pytest.skip()
    else:
        gdaltest.pcidsk_new = 1

    # Create testing file.

    gdaltest.pcidsk_ds = driver.Create('tmp/pcidsk_5.pix', 400, 600, 1,
                                       gdal.GDT_Byte)

    # Write out some metadata to the default and non-default domain and
    # using the set and single methods.

    gdaltest.pcidsk_ds.SetMetadata(['ABC=DEF', 'GHI=JKL'])
    gdaltest.pcidsk_ds.SetMetadataItem('XXX', 'YYY')
    gdaltest.pcidsk_ds.SetMetadataItem('XYZ', '123', 'AltDomain')

    # Close and reopen.
    gdaltest.pcidsk_ds = None
    gdaltest.pcidsk_ds = gdal.Open('tmp/pcidsk_5.pix', gdal.GA_Update)

    # Check metadata.
    mddef = gdaltest.pcidsk_ds.GetMetadata()
    if mddef['GHI'] != 'JKL' or mddef['XXX'] != 'YYY':
        print(mddef)
        gdaltest.post_reason('file default domain metadata broken. ')

    assert gdaltest.pcidsk_ds.GetMetadataItem('GHI') == 'JKL'
    assert gdaltest.pcidsk_ds.GetMetadataItem('GHI') == 'JKL'
    gdaltest.pcidsk_ds.SetMetadataItem('GHI', 'JKL2')
    assert gdaltest.pcidsk_ds.GetMetadataItem('GHI') == 'JKL2'
    assert gdaltest.pcidsk_ds.GetMetadataItem('I_DONT_EXIST') is None
    assert gdaltest.pcidsk_ds.GetMetadataItem('I_DONT_EXIST') is None

    mdalt = gdaltest.pcidsk_ds.GetMetadata('AltDomain')
    if mdalt['XYZ'] != '123':
        print(mdalt)
        gdaltest.post_reason('file alt domain metadata broken. ')

    
###############################################################################
# Test writing metadata to a band.


def test_pcidsk_6():

    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    # Write out some metadata to the default and non-default domain and
    # using the set and single methods.
    band = gdaltest.pcidsk_ds.GetRasterBand(1)

    band.SetMetadata(['ABC=DEF', 'GHI=JKL'])
    band.SetMetadataItem('XXX', 'YYY')
    band.SetMetadataItem('XYZ', '123', 'AltDomain')
    band = None

    # Close and reopen.
    gdaltest.pcidsk_ds = None
    gdaltest.pcidsk_ds = gdal.Open('tmp/pcidsk_5.pix', gdal.GA_Update)

    # Check metadata.
    band = gdaltest.pcidsk_ds.GetRasterBand(1)
    mddef = band.GetMetadata()
    if mddef['GHI'] != 'JKL' or mddef['XXX'] != 'YYY':
        print(mddef)
        gdaltest.post_reason('channel default domain metadata broken. ')

    assert band.GetMetadataItem('GHI') == 'JKL'
    assert band.GetMetadataItem('GHI') == 'JKL'
    band.SetMetadataItem('GHI', 'JKL2')
    assert band.GetMetadataItem('GHI') == 'JKL2'
    assert band.GetMetadataItem('I_DONT_EXIST') is None
    assert band.GetMetadataItem('I_DONT_EXIST') is None

    mdalt = band.GetMetadata('AltDomain')
    if mdalt['XYZ'] != '123':
        print(mdalt)
        gdaltest.post_reason('channel alt domain metadata broken. ')

    
###############################################################################
# Test creating a color table and reading it back.


def test_pcidsk_7():

    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    # Write out some metadata to the default and non-default domain and
    # using the set and single methods.
    band = gdaltest.pcidsk_ds.GetRasterBand(1)

    ct = band.GetColorTable()

    assert ct is None, 'Got color table unexpectedly.'

    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (0, 255, 0, 255))
    ct.SetColorEntry(1, (255, 0, 255, 255))
    ct.SetColorEntry(2, (0, 0, 255, 255))
    band.SetColorTable(ct)

    ct = band.GetColorTable()

    assert ct.GetColorEntry(1) == (255, 0, 255, 255), \
        'Got wrong color table entry immediately.'

    ct = None
    band = None

    # Close and reopen.
    gdaltest.pcidsk_ds = None
    gdaltest.pcidsk_ds = gdal.Open('tmp/pcidsk_5.pix', gdal.GA_Update)

    band = gdaltest.pcidsk_ds.GetRasterBand(1)

    ct = band.GetColorTable()

    assert ct.GetColorEntry(1) == (255, 0, 255, 255), \
        'Got wrong color table entry after reopen.'

    assert band.GetColorInterpretation() == gdal.GCI_PaletteIndex, 'Not a palette?'

    assert band.SetColorTable(None) == 0, 'SetColorTable failed.'

    assert band.GetColorTable() is None, 'color table still exists!'

    assert band.GetColorInterpretation() == gdal.GCI_Undefined, 'Paletted?'

###############################################################################
# Test FILE interleaving.


def test_pcidsk_8():

    tst = gdaltest.GDALTest('PCIDSK', 'png/rgba16.png', 2, 2042,
                            options=['INTERLEAVING=FILE'])

    return tst.testCreate()

###############################################################################
# Test that we cannot open a vector only pcidsk
# FIXME: test disabled because of unification


def pcidsk_9():

    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    ogr_drv = ogr.GetDriverByName('PCIDSK')
    if ogr_drv is None:
        pytest.skip()

    ds = ogr_drv.CreateDataSource('/vsimem/pcidsk_9.pix')
    ds.CreateLayer('foo')
    ds = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('/vsimem/pcidsk_9.pix')
    gdal.PopErrorHandler()
    assert ds is None
    ds = None

    gdal.Unlink('/vsimem/pcidsk_9.pix')

###############################################################################
# Test overview creation.


def test_pcidsk_10():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('PCIDSK').CreateCopy('/vsimem/pcidsk_10.pix', src_ds)
    src_ds = None

    # ds = None
    # ds = gdal.Open( '/vsimem/pcidsk_10.pix', gdal.GA_Update )

    band = ds.GetRasterBand(1)
    ds.BuildOverviews('NEAR', [2])

    assert band.GetOverviewCount() == 1, 'did not get expected overview count'

    cs = band.GetOverview(0).Checksum()
    assert cs == 1087, ('wrong overview checksum (%d)' % cs)

    ds = None

    gdal.GetDriverByName('PCIDSK').Delete('/vsimem/pcidsk_10.pix')

###############################################################################
# Test INTERLEAVING=TILED interleaving.


def test_pcidsk_11():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    tst = gdaltest.GDALTest('PCIDSK', 'png/rgba16.png', 2, 2042,
                            options=['INTERLEAVING=TILED', 'TILESIZE=32'])

    return tst.testCreate()

def test_pcidsk_11_v1():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    tst = gdaltest.GDALTest('PCIDSK', 'png/rgba16.png', 2, 2042,
                            options=['INTERLEAVING=TILED', 'TILESIZE=32', 'TILEVERSION=1'])

    return tst.testCreate()

def test_pcidsk_11_v2():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    tst = gdaltest.GDALTest('PCIDSK', 'png/rgba16.png', 2, 2042,
                            options=['INTERLEAVING=TILED', 'TILESIZE=32', 'TILEVERSION=2'])

    return tst.testCreate()

###############################################################################
# Test INTERLEAVING=TILED interleaving and COMPRESSION=RLE


def test_pcidsk_12():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    tst = gdaltest.GDALTest('PCIDSK', 'png/rgba16.png', 2, 2042,
                            options=['INTERLEAVING=TILED', 'TILESIZE=32', 'COMPRESSION=RLE'])

    return tst.testCreate()

def test_pcidsk_12_v1():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    tst = gdaltest.GDALTest('PCIDSK', 'png/rgba16.png', 2, 2042,
                            options=['INTERLEAVING=TILED', 'TILESIZE=32', 'COMPRESSION=RLE', 'TILEVERSION=1'])

    return tst.testCreate()

def test_pcidsk_12_v2():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    tst = gdaltest.GDALTest('PCIDSK', 'png/rgba16.png', 2, 2042,
                            options=['INTERLEAVING=TILED', 'TILESIZE=32', 'COMPRESSION=RLE', 'TILEVERSION=2'])

    return tst.testCreate()

###############################################################################
# Test INTERLEAVING=TILED interleaving and COMPRESSION=JPEG


def test_pcidsk_13():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    if gdal.GetDriverByName('JPEG') is None:
        pytest.skip()

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('PCIDSK').CreateCopy('/vsimem/pcidsk_13.pix', src_ds, options=['INTERLEAVING=TILED', 'COMPRESSION=JPEG'])
    src_ds = None

    gdal.Unlink('/vsimem/pcidsk_13.pix.aux.xml')

    ds = None
    ds = gdal.Open('/vsimem/pcidsk_13.pix')
    band = ds.GetRasterBand(1)
    band.GetDescription()
    cs = band.Checksum()
    ds = None

    gdal.GetDriverByName('PCIDSK').Delete('/vsimem/pcidsk_13.pix')

    assert cs == 4645, 'bad checksum'

###############################################################################
# Test SetDescription()


def test_pcidsk_14():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    ds = gdal.GetDriverByName('PCIDSK').Create('/vsimem/pcidsk_14.pix', 1, 1)
    band = ds.GetRasterBand(1).SetDescription('mydescription')
    del ds

    gdal.Unlink('/vsimem/pcidsk_14.pix.aux.xml')

    ds = None
    ds = gdal.Open('/vsimem/pcidsk_14.pix')
    band = ds.GetRasterBand(1)
    desc = band.GetDescription()
    ds = None

    gdal.GetDriverByName('PCIDSK').Delete('/vsimem/pcidsk_14.pix')

    assert desc == 'mydescription', 'bad description'

###############################################################################
# Test mixed raster and vector


def test_pcidsk_15():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    # One raster band and vector layer
    ds = gdal.GetDriverByName('PCIDSK').Create('/vsimem/pcidsk_15.pix', 1, 1)
    ds.CreateLayer('foo')
    ds = None

    ds = gdal.Open('/vsimem/pcidsk_15.pix')
    assert ds.RasterCount == 1
    assert ds.GetLayerCount() == 1

    ds2 = gdal.GetDriverByName('PCIDSK').CreateCopy('/vsimem/pcidsk_15_2.pix', ds)
    ds2 = None
    ds = None

    ds = gdal.Open('/vsimem/pcidsk_15_2.pix')
    assert ds.RasterCount == 1
    assert ds.GetLayerCount() == 1
    ds = None

    # One vector layer only
    ds = gdal.GetDriverByName('PCIDSK').Create('/vsimem/pcidsk_15.pix', 0, 0, 0)
    ds.CreateLayer('foo')
    ds = None

    ds = gdal.OpenEx('/vsimem/pcidsk_15.pix')
    assert ds.RasterCount == 0
    assert ds.GetLayerCount() == 1

    ds2 = gdal.GetDriverByName('PCIDSK').CreateCopy('/vsimem/pcidsk_15_2.pix', ds)
    ds2 = None
    ds = None

    ds = gdal.OpenEx('/vsimem/pcidsk_15_2.pix')
    assert ds.RasterCount == 0
    assert ds.GetLayerCount() == 1
    ds = None

    # Zero raster band and vector layer
    ds = gdal.GetDriverByName('PCIDSK').Create('/vsimem/pcidsk_15.pix', 0, 0, 0)
    ds = None

    ds = gdal.OpenEx('/vsimem/pcidsk_15.pix')
    assert ds.RasterCount == 0
    assert ds.GetLayerCount() == 0

    ds2 = gdal.GetDriverByName('PCIDSK').CreateCopy('/vsimem/pcidsk_15_2.pix', ds)
    del ds2
    ds = None

    ds = gdal.OpenEx('/vsimem/pcidsk_15_2.pix')
    assert ds.RasterCount == 0
    assert ds.GetLayerCount() == 0
    ds = None

    gdal.GetDriverByName('PCIDSK').Delete('/vsimem/pcidsk_15.pix')
    gdal.GetDriverByName('PCIDSK').Delete('/vsimem/pcidsk_15_2.pix')

###############################################################################


def test_pcidsk_external_ovr():

    gdal.Translate('/vsimem/test.pix', 'data/byte.tif', format='PCIDSK')
    ds = gdal.Open('/vsimem/test.pix')
    ds.BuildOverviews('NEAR', [2])
    ds = None
    assert gdal.VSIStatL('/vsimem/test.pix.ovr') is not None
    ds = gdal.Open('/vsimem/test.pix')
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    ds = None

    gdal.GetDriverByName('PCIDSK').Delete('/vsimem/test.pix')

###############################################################################


def test_pcidsk_external_ovr_rrd():

    gdal.Translate('/vsimem/test.pix', 'data/byte.tif', format='PCIDSK')
    ds = gdal.Open('/vsimem/test.pix', gdal.GA_Update)
    with gdaltest.config_option('USE_RRD', 'YES'):
        ds.BuildOverviews('NEAR', [2])
    ds = None
    assert gdal.VSIStatL('/vsimem/test.aux') is not None
    ds = gdal.Open('/vsimem/test.pix')
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    ds = None

    gdal.GetDriverByName('PCIDSK').Delete('/vsimem/test.pix')

###############################################################################
# Check various items from a modern irvine.pix


def test_pcidsk_online_1():
    if gdaltest.pcidsk_new == 0:
        pytest.skip()

    if not gdaltest.download_file('http://download.osgeo.org/gdal/data/pcidsk/sdk_testsuite/irvine_gcp2.pix', 'irvine_gcp2.pix'):
        pytest.skip()

    ds = gdal.Open('tmp/cache/irvine_gcp2.pix')

    band = ds.GetRasterBand(6)

    names = band.GetRasterCategoryNames()

    exp_names = ['', '', '', '', '', '', '', '', '', '', '', 'Residential', 'Commercial', 'Industrial', 'Transportation', 'Commercial/Industrial', 'Mixed', 'Other', '', '', '', 'Crop/Pasture', 'Orchards', 'Feeding', 'Other', '', '', '', '', '', '', 'Herbaceous', 'Shrub', 'Mixed', '', '', '', '', '', '', '', 'Deciduous', 'Evergreen', 'Mixed', '', '', '', '', '', '', '', 'Streams/Canals', 'Lakes', 'Reservoirs', 'Bays/Estuaries', '', '', '', '', '', '', 'Forested', 'Nonforested', '', '', '', '', '', '', '', '', 'Dry_Salt_Flats', 'Beaches', 'Sandy_Areas', 'Exposed_Rock', 'Mines/Quarries/Pits', 'Transitional_Area', 'Mixed', '', '', '', 'Shrub/Brush', 'Herbaceous', 'Bare', 'Wet', 'Mixed', '', '', '', '', '', 'Perennial_Snow', 'Glaciers']

    if names != exp_names:
        print(names)
        gdaltest.post_reason('did not get expected category names.')
        return 'false'

    band = ds.GetRasterBand(20)
    assert band.GetDescription() == 'Training site for type 2 crop', \
        'did not get expected band 20 description'

    exp_checksum = 2057
    checksum = band.Checksum()
    assert exp_checksum == checksum, 'did not get right bitmap checksum.'

    md = band.GetMetadata('IMAGE_STRUCTURE')
    assert md['NBITS'] == '1', 'did not get expected NBITS=1 metadata.'

###############################################################################
# Read test of a PCIDSK TILED version 1 file.

def test_pcidsk_tile_v1():

    tst = gdaltest.GDALTest('PCIDSK', 'pcidsk/tile_v1.1.pix', 1, 49526)

    return tst.testCreateCopy(check_gt=1, check_srs=1)

def test_pcidsk_tile_v1_overview():

    ds = gdal.Open('data/pcidsk/tile_v1.1.pix')

    band = ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 1, 'did not get expected overview count'

    cs = band.GetOverview(0).Checksum()
    assert cs == 12003, ('wrong overview checksum (%d)' % cs)

###############################################################################
# Read test of a PCIDSK TILED version 2 file.

def test_pcidsk_tile_v2():

    tst = gdaltest.GDALTest('PCIDSK', 'pcidsk/tile_v2.pix', 1, 49526)

    return tst.testCreateCopy(check_gt=1, check_srs=1)

def test_pcidsk_tile_v2_overview():

    ds = gdal.Open('data/pcidsk/tile_v2.pix')

    band = ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 1, 'did not get expected overview count'

    cs = band.GetOverview(0).Checksum()
    assert cs == 12003, ('wrong overview checksum (%d)' % cs)

###############################################################################
# Cleanup.


def test_pcidsk_cleanup():
    gdaltest.pcidsk_ds = None
    gdaltest.clean_tmp()



