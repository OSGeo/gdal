#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for MRF driver.
# Author:   Even Rouault, <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2016, Even Rouault, <even.rouault at spatialys.com>
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

import glob
import pytest
from osgeo import gdal
import gdaltest

pytestmark = pytest.mark.require_driver('MRF')

mrf_list = [
    ('byte.tif', 4672, [4672], []),
    ('byte.tif', 4672, [4672], ['COMPRESS=ZSTD']),
    ('byte.tif', 4672, [4672], ['COMPRESS=DEFLATE']),
    ('byte.tif', 4672, [4672], ['COMPRESS=NONE']),
    ('byte.tif', 4672, [4672], ['COMPRESS=LERC']),
    ('byte.tif', 4672, [5015], ['COMPRESS=LERC', 'OPTIONS:LERC_PREC=10']),
    ('byte.tif', 4672, [4672], ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('int16.tif', 4672, [4672], []),
    ('int16.tif', 4672, [4672], ['COMPRESS=ZSTD']),
    ('int16.tif', 4672, [4672], ['COMPRESS=DEFLATE']),
    ('int16.tif', 4672, [4672], ['COMPRESS=LERC']),
    ('int16.tif', 4672, [4672], ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('../../gcore/data/uint16.tif', 4672, [4672], []),
    ('../../gcore/data/uint16.tif', 4672, [4672], ['COMPRESS=DEFLATE']),
    ('../../gcore/data/uint16.tif', 4672, [4672], ['COMPRESS=ZSTD']),
    ('../../gcore/data/uint16.tif', 4672, [4672], ['COMPRESS=LERC']),
    ('../../gcore/data/uint16.tif', 4672, [4672], ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('int32.tif', 4672, [4672], ['COMPRESS=DEFLATE']),
    ('int32.tif', 4672, [4672], ['COMPRESS=ZSTD']),
    ('int32.tif', 4672, [4672], ['COMPRESS=TIF']),
    ('int32.tif', 4672, [4672], ['COMPRESS=LERC']),
    ('int32.tif', 4672, [4672], ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('../../gcore/data/uint32.tif', 4672, [4672], ['COMPRESS=DEFLATE']),
    ('../../gcore/data/uint32.tif', 4672, [4672], ['COMPRESS=ZSTD']),
    ('../../gcore/data/uint32.tif', 4672, [4672], ['COMPRESS=TIF']),
    ('../../gcore/data/uint32.tif', 4672, [4672], ['COMPRESS=LERC']),
    ('../../gcore/data/uint32.tif', 4672, [4672], ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('float32.tif', 4672, [4672], ['COMPRESS=DEFLATE']),
    ('float32.tif', 4672, [4672], ['COMPRESS=ZSTD']),
    ('float32.tif', 4672, [4672], ['COMPRESS=TIF']),
    ('float32.tif', 4672, [4672], ['COMPRESS=LERC']),
    ('float32.tif', 4672, [4672], ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('float64.tif', 4672, [4672], ['COMPRESS=DEFLATE']),
    ('float64.tif', 4672, [4672], ['COMPRESS=ZSTD']),
    ('float64.tif', 4672, [4672], ['COMPRESS=TIF']),
    ('float64.tif', 4672, [4672], ['COMPRESS=LERC']),
    ('float64.tif', 4672, [5015], ['COMPRESS=LERC', 'OPTIONS:LERC_PREC=10']),
    ('float64.tif', 4672, [4672], ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('../../gcore/data/utmsmall.tif', 50054, [50054], []),
    ('small_world.tif', 30111, [30111], ['COMPRESS=ZSTD']),
    ('small_world.tif', 30111, [30111], ['COMPRESS=ZSTD', 'INTERLEAVE=PIXEL']),
    ('small_world.tif', 30111, [30111], ['COMPRESS=LERC', 'INTERLEAVE=PIXEL']),
    ('small_world.tif', 30111, [30111], ['COMPRESS=LERC', 'OPTIONS=V1:1', 'INTERLEAVE=PIXEL']),
    ('small_world_pct.tif', 14890, [14890], ['COMPRESS=PPNG']),
    ('byte.tif', 4672, [4603, 4652], ['COMPRESS=JPEG', 'QUALITY=99']),
    # following expected checksums are for: gcc 4.4 debug, mingw/vc9 32-bit, mingw-w64/vc12 64bit, MacOSX
    ('rgbsmall.tif', 21212, [21162, 21110, 21155, 21116], ['COMPRESS=JPEG', 'QUALITY=99']),
    ('rgbsmall.tif', 21212, [21266, 21369, 21256, 21495], ['INTERLEAVE=PIXEL', 'COMPRESS=JPEG', 'QUALITY=99']),
    ('rgbsmall.tif', 21212, [21261, 21209, 21254, 21215], ['INTERLEAVE=PIXEL', 'COMPRESS=JPEG', 'QUALITY=99', 'PHOTOMETRIC=RGB']),
    ('rgbsmall.tif', 21212, [21283, 21127, 21278, 21124], ['INTERLEAVE=PIXEL', 'COMPRESS=JPEG', 'QUALITY=99', 'PHOTOMETRIC=YCC']),
    ('jpeg/12bit_rose_extract.jpg', 30075, [29650, 29680, 29680, 29650], ['COMPRESS=JPEG']),
    # checksum depends on floating point precision
    ('f32nan_data.tif', 54061, [54052, 54050], ['COMPRESS=LERC', 'OPTIONS=V1:Yes LERC_PREC:0.01']),
]

@pytest.mark.parametrize(
    'src_filename,chksum,chksum_after_reopening,options',
    mrf_list,
    ids=['{0}-{3}'.format(*r) for r in mrf_list],
)
def test_mrf(src_filename, chksum, chksum_after_reopening, options):

    mrf_co = gdal.GetDriverByName('MRF').GetMetadataItem('DMD_CREATIONOPTIONLIST')

    if 'COMPRESS=LERC' in options and 'LERC' not in mrf_co:
        pytest.skip()

    if 'COMPRESS=ZSTD' in options and 'ZSTD' not in mrf_co:
        pytest.skip()

    if 'jpg' in src_filename:
        import jpeg
        jpeg.test_jpeg_1()
        if gdaltest.jpeg_version == '9b':
            pytest.skip()

    with gdaltest.error_handler():
        ds = gdal.Open('data/' + src_filename)
    if ds is None:
        pytest.skip()

    ds = None
    ut = gdaltest.GDALTest('MRF', src_filename, 1, chksum, options=options, chksum_after_reopening=chksum_after_reopening)

    check_minmax = 'COMPRESS=JPEG' not in ut.options
    for x in ut.options:
        if 'OPTIONS:LERC_PREC=' in x:
            check_minmax = False
    return ut.testCreateCopy(check_minmax=check_minmax)

def cleanup(base = '/vsimem/out.'):
    for ext in ['mrf', 'mrf.aux.xml', 'idx', 'ppg', 'til', 'lrc', 'pjg', 'pzp', 'psz']:
        gdal.Unlink(base + ext)

def test_mrf_zen_test():
    result = 'success'
    expectedCS = 770
    testvrt = '''
<VRTDataset rasterXSize="512" rasterYSize="512">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <ComplexSource>
      <SourceFilename relativeToVRT="0">tmp/masked.mrf</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="512" RasterYSize="512" DataType="Byte" BlockXSize="512" BlockYSize="512" />
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <DstRect xOff="0" yOff="0" xSize="512" ySize="512" />
      <LUT>0:0,1:255,255:255</LUT>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>
'''
    for interleave in 'PIXEL', 'BAND':
        co = ['COMPRESS=JPEG', 'INTERLEAVE=' + interleave]
        gdal.Translate('tmp/masked.mrf', 'data/jpeg/masked.jpg', format='MRF', creationOptions=co)
        ds = gdal.Open(testvrt)
        cs = ds.GetRasterBand(1).Checksum()
        if cs != expectedCS:
            gdaltest.post_reason('Interleave=' + interleave +
                                 ' expected checksum ' + str(expectedCS) + ' got ' + str(cs))
            result = 'fail'
        for f in glob.glob('tmp/masked.*'):
            gdal.Unlink(f)

    return result


def test_mrf_overview_nnb_fact_2():

    expected_cs = 1087
    for dt in [gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
               gdal.GDT_Int32, gdal.GDT_UInt32,
               gdal.GDT_Float32, gdal.GDT_Float64]:

        out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                                format='MRF',
                                creationOptions=['COMPRESS=NONE', 'BLOCKSIZE=10'],
                                outputType=dt)
        out_ds.BuildOverviews('NEARNB', [2])
        out_ds = None

        ds = gdal.Open('/vsimem/out.mrf')
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        assert cs == expected_cs, gdal.GetDataTypeName(dt)
        ds = None
        cleanup()


def test_mrf_overview_nnb_with_nodata_fact_2():

    expected_cs = 1117
    for dt in [gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
               gdal.GDT_Int32, gdal.GDT_UInt32,
               gdal.GDT_Float32, gdal.GDT_Float64]:

        out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                                format='MRF',
                                creationOptions=['COMPRESS=NONE', 'BLOCKSIZE=10'],
                                outputType=dt,
                                noData=107)
        out_ds.BuildOverviews('NNB', [2])
        out_ds = None

        ds = gdal.Open('/vsimem/out.mrf')
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        assert cs == expected_cs, gdal.GetDataTypeName(dt)
        ds = None
        cleanup()


def test_mrf_overview_avg_fact_2():

    for dt in [gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
               gdal.GDT_Int32, gdal.GDT_UInt32,
               gdal.GDT_Float32, gdal.GDT_Float64]:

        out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                                format='MRF',
                                creationOptions=['COMPRESS=NONE', 'BLOCKSIZE=10'],
                                outputType=dt)
        out_ds.BuildOverviews('AVG', [2])
        out_ds = None

        expected_cs = 1152

        ds = gdal.Open('/vsimem/out.mrf')
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        assert cs == expected_cs, gdal.GetDataTypeName(dt)
        ds = None
        cleanup()


def test_mrf_overview_avg_with_nodata_fact_2():

    for dt in [gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
               gdal.GDT_Int32, gdal.GDT_UInt32,
               gdal.GDT_Float32, gdal.GDT_Float64]:

        out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                                format='MRF',
                                creationOptions=['COMPRESS=NONE', 'BLOCKSIZE=10'],
                                outputType=dt,
                                noData=107)
        out_ds.BuildOverviews('AVG', [2])
        out_ds = None

        expected_cs = 1164

        ds = gdal.Open('/vsimem/out.mrf')
        cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
        assert cs == expected_cs, gdal.GetDataTypeName(dt)
        ds = None
        cleanup()


def test_mrf_nnb_overview_partial_block():

    out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format='MRF',
                            creationOptions=['COMPRESS=NONE', 'BLOCKSIZE=8'])
    out_ds.BuildOverviews('NNB', [2])
    out_ds = None

    ds = gdal.Open('/vsimem/out.mrf')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 1087
    ds = None
    cleanup()


def test_mrf_overview_nnb_implicit_level():

    expected_cs = 93
    # We ask for overview level 2 and 4, triggering full overviews
    # so check that 8 is properly initialized
    out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format='MRF',
                            creationOptions=['COMPRESS=NONE', 'BLOCKSIZE=4'])
    out_ds.BuildOverviews('NNB', [2, 4])
    out_ds = None

    ds = gdal.Open('/vsimem/out.mrf')
    cs = ds.GetRasterBand(1).GetOverview(2).Checksum()
    assert cs == expected_cs
    ds = None

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.mrf:MRF:L3')
    assert ds is None

    ds = gdal.Open('/vsimem/out.mrf:MRF:L2')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == expected_cs
    ds = None
    cleanup()


def test_mrf_overview_external():

    gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format='MRF')
    ds = gdal.Open('/vsimem/out.mrf')
    ds.BuildOverviews('NEAR', [2])
    ds = None

    ds = gdal.Open('/vsimem/out.mrf')
    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 1087
    assert cs == expected_cs
    ds = None
    cleanup()


def test_mrf_lerc_nodata():

    mrf_co = gdal.GetDriverByName('MRF').GetMetadataItem('DMD_CREATIONOPTIONLIST')
    if 'LERC' not in mrf_co:
        pytest.skip()

    gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format='MRF',
                   noData=107, creationOptions=['COMPRESS=LERC'])
    ds = gdal.Open('/vsimem/out.mrf')
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    assert nodata == 107
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    assert cs == expected_cs
    ds = None
    cleanup()


def test_mrf_lerc_with_huffman():

    mrf_co = gdal.GetDriverByName('MRF').GetMetadataItem('DMD_CREATIONOPTIONLIST')
    if 'LERC' not in mrf_co:
        pytest.skip()

    gdal.Translate('/vsimem/out.mrf', 'data/small_world.tif', format='MRF',
                   width=5000, height=5000, creationOptions=['COMPRESS=LERC'])
    ds = gdal.Open('/vsimem/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 31204
    assert cs == expected_cs
    ds = None
    cleanup()

def test_raw_lerc():

    mrf_co = gdal.GetDriverByName('MRF').GetMetadataItem('DMD_CREATIONOPTIONLIST')
    if 'LERC' not in  mrf_co:
        pytest.skip()

    # Defaults to LERC2
    for opt in 'OPTIONS=V1:1', None:
        co = ['COMPRESS=LERC']
        if opt:
            co.append(opt)
        gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format='MRF',
                        creationOptions = co)
        ds = gdal.Open('/vsimem/out.lrc')
        with gdaltest.error_handler():
            cs = ds.GetRasterBand(1).Checksum()
        expected_cs = 4819
        assert cs == expected_cs
        ds = None
        # Test open options for raw LERC1, it accepts NDV and datatype overrides
        if opt:
            ds = gdal.OpenEx('/vsimem/out.lrc', open_options = ['@NDV=100, @datatype=UInt32'])
            with gdaltest.error_handler():
                cs = ds.GetRasterBand(1).Checksum()
            print(cs, opt)
            assert cs == 60065
            ds = None
        cleanup()

def test_mrf_cached_source():

    # Test empty cache creation
    gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format='MRF',
                   creationOptions=['CACHEDSOURCE=invalid_source', 'NOCOPY=TRUE'])
    ds = gdal.Open('/vsimem/out.mrf')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 0
    assert cs == expected_cs
    ds = None
    cleanup()

    open('tmp/byte.tif', 'wb').write(open('data/byte.tif', 'rb').read())
    gdal.Translate('tmp/out.mrf', 'tmp/byte.tif', format='MRF',
                   creationOptions=['CACHEDSOURCE=byte.tif', 'NOCOPY=TRUE'])
    ds = gdal.Open('tmp/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    assert cs == expected_cs
    ds = None

    gdal.Unlink('tmp/byte.tif')
    ds = gdal.Open('tmp/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    assert cs == expected_cs
    ds = None
    cleanup('tmp/out.')

    # Caching MRF in mp_safe mode
    open('tmp/byte.tif', 'wb').write(open('data/byte.tif', 'rb').read())
    open('tmp/out.mrf', 'wt').write(
"""<MRF_META>
  <CachedSource>
    <Source>byte.tif</Source>
  </CachedSource>
  <Raster mp_safe="on">
    <Size x="20" y="20" c="1" />
    <PageSize x="512" y="512" c="1" />
  </Raster>
</MRF_META>""")
    ds = gdal.Open('tmp/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    assert cs == expected_cs
    ds = None

    # Read it again, from the cache
    gdal.Unlink('tmp/byte.tif')
    ds = gdal.Open('tmp/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    assert cs == expected_cs
    ds = None
    # No cleanup, will test cloning next

    # Cloning MRF
    open('tmp/cloning.mrf', 'wt').write(
"""<MRF_META>
  <CachedSource>
    <Source clone="true">out.mrf</Source>
  </CachedSource>
  <Raster>
    <Size x="20" y="20" c="1" />
    <PageSize x="512" y="512" c="1" />
  </Raster>
</MRF_META>""")
    ds = gdal.Open('tmp/cloning.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    assert cs == expected_cs
    ds = None
    cleanup('tmp/out.')

    # Read it again, from the cache
    ds = gdal.Open('tmp/cloning.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    assert cs == expected_cs
    ds = None
    cleanup('tmp/cloning.')

def test_mrf_versioned():

    # Caching MRF
    gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format='MRF')
    gdal.FileFromMemBuffer('/vsimem/out.mrf',
"""<MRF_META>
  <Raster versioned="on">
    <Size x="20" y="20" c="1" />
    <PageSize x="512" y="512" c="1" />
  </Raster>
</MRF_META>""")
    ds = gdal.Open('/vsimem/out.mrf', gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = gdal.Open('/vsimem/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 0
    assert cs == expected_cs
    ds = None

    ds = gdal.Open('/vsimem/out.mrf:MRF:V0')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 0
    assert cs == expected_cs
    ds = None

    ds = gdal.Open('/vsimem/out.mrf:MRF:V1')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    assert cs == expected_cs
    ds = None

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.mrf:MRF:V2')
    assert ds is None

    cleanup()


def test_mrf_cleanup():

    files = [
        '12bit_rose_extract.jpg.*',
        'byte.tif.*',
        'int16.tif.*',
        'rgbsmall.tif.*',
        'small_world*',
        'float32.tif.*',
        'float64.tif.*',
        'int32.tif.*',
        'uint16.tif.*',
        'uint32.tif.*',
        'utmsmall.tif.*',
        'cloning.*',
        'f32nan_data.*'
        ]

    for f in [fname for n in files for fname in glob.glob('tmp/' + n)]:
        gdal.Unlink(f)

    cleanup()
    cleanup('tmp/out.')
