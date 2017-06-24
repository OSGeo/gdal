#!/usr/bin/env python
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

import sys

sys.path.append( '../pymod' )

from osgeo import gdal

import gdaltest

init_list = [
    ('byte.tif', 1, 4672, None),
    ('byte.tif', 1, 4672, ['COMPRESS=DEFLATE']),
    ('byte.tif', 1, 4672, ['COMPRESS=NONE']),
    ('byte.tif', 1, 4672, ['COMPRESS=LERC']),
    ('byte.tif', 1, [4672, 5015], ['COMPRESS=LERC', 'OPTIONS:LERC_PREC=10']),
    ('byte.tif', 1, 4672, ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('int16.tif', 1, 4672, None),
    ('int16.tif', 1, 4672, ['COMPRESS=LERC']),
    ('int16.tif', 1, 4672, ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('../../gcore/data/uint16.tif', 1, 4672, None),
    ('../../gcore/data/uint16.tif', 1, 4672, ['COMPRESS=LERC']),
    ('../../gcore/data/uint16.tif', 1, 4672, ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('../../gcore/data/int32.tif', 1, 4672, ['COMPRESS=TIF']),
    ('../../gcore/data/int32.tif', 1, 4672, ['COMPRESS=LERC']),
    ('../../gcore/data/int32.tif', 1, 4672, ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('../../gcore/data/uint32.tif', 1, 4672, ['COMPRESS=TIF']),
    ('../../gcore/data/uint32.tif', 1, 4672, ['COMPRESS=LERC']),
    ('../../gcore/data/uint32.tif', 1, 4672, ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('../../gcore/data/float32.tif', 1, 4672, ['COMPRESS=TIF']),
    ('../../gcore/data/float32.tif', 1, 4672, ['COMPRESS=LERC']),
    ('../../gcore/data/float32.tif', 1, 4672, ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('../../gcore/data/float64.tif', 1, 4672, ['COMPRESS=TIF']),
    ('../../gcore/data/float64.tif', 1, 4672, ['COMPRESS=LERC']),
    ('../../gcore/data/float64.tif', 1, [4672, 5015], ['COMPRESS=LERC', 'OPTIONS:LERC_PREC=10']),
    ('../../gcore/data/float64.tif', 1, 4672, ['COMPRESS=LERC', 'OPTIONS=V1:YES']),
    ('../../gcore/data/utmsmall.tif', 1, 50054, None),
    ('small_world_pct.tif', 1, 14890, ['COMPRESS=PPNG']),
    ('byte.tif', 1, [4672, [4652,4603]], ['COMPRESS=JPEG', 'QUALITY=99']),
    # following expected checksums are for: gcc 4.4 debug, mingw/vc9 32-bit, mingw-w64/vc12 64bit, MacOSX
    ('rgbsmall.tif', 1, [21212, [21137,21223,21231,21150]], ['COMPRESS=JPEG', 'QUALITY=99']),
    ('rgbsmall.tif', 1, [21212, [21333,21256,21264,21510]], ['INTERLEAVE=PIXEL','COMPRESS=JPEG', 'QUALITY=99']),
    ('rgbsmall.tif', 1, [21212, [21137,21223,21231,21150]], ['INTERLEAVE=PIXEL','COMPRESS=JPEG', 'QUALITY=99','PHOTOMETRIC=RGB']),
    ('rgbsmall.tif', 1, [21212, [21061,21240,21243,21060]], ['INTERLEAVE=PIXEL','COMPRESS=JPEG', 'QUALITY=99','PHOTOMETRIC=YCC']),
    ('12bit_rose_extract.jpg', 1, [30075, [29650,29680,29680,29650]], ['COMPRESS=JPEG']),
]

def mrf_overview_near_fact_2():

    ref_ds = gdal.Translate('/vsimem/out.tif', 'data/byte.tif')
    ref_ds.BuildOverviews('NEAR', [2])
    expected_cs = ref_ds.GetRasterBand(1).GetOverview(0).Checksum()
    ref_ds = None
    gdal.Unlink('/vsimem/out.tif')

    for dt in [ gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
                gdal.GDT_Int32, gdal.GDT_UInt32,
                gdal.GDT_Float32, gdal.GDT_Float64 ]:

        out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                                format = 'MRF',
                                creationOptions = ['COMPRESS=NONE', 'BLOCKSIZE=10'],
                                outputType = dt)
        out_ds.BuildOverviews('NEAR', [2])
        out_ds = None

        ds = gdal.Open('/vsimem/out.mrf')
        cs= ds.GetRasterBand(1).GetOverview(0).Checksum()
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(dt)
            print(cs)
            print(expected_cs)
            return 'fail'
        ds = None

        gdal.Unlink('/vsimem/out.mrf')
        gdal.Unlink('/vsimem/out.mrf.aux.xml')
        gdal.Unlink('/vsimem/out.idx')
        gdal.Unlink('/vsimem/out.ppg')
        gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_overview_near_with_nodata_fact_2():

    for dt in [ gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
                gdal.GDT_Int32, gdal.GDT_UInt32,
                gdal.GDT_Float32, gdal.GDT_Float64 ]:

        out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                                format = 'MRF',
                                creationOptions = ['COMPRESS=NONE', 'BLOCKSIZE=10'],
                                outputType = dt,
                                noData = 107)
        out_ds.BuildOverviews('NEAR', [2])
        out_ds = None

        ds = gdal.Open('/vsimem/out.mrf')
        cs= ds.GetRasterBand(1).GetOverview(0).Checksum()
        expected_cs = 1117
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(dt)
            print(cs)
            print(expected_cs)
            return 'fail'
        ds = None

        gdal.Unlink('/vsimem/out.mrf')
        gdal.Unlink('/vsimem/out.mrf.aux.xml')
        gdal.Unlink('/vsimem/out.idx')
        gdal.Unlink('/vsimem/out.ppg')
        gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_overview_avg_fact_2():

    for dt in [ gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
                gdal.GDT_Int32, gdal.GDT_UInt32,
                gdal.GDT_Float32, gdal.GDT_Float64 ]:

        out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                                format = 'MRF',
                                creationOptions = ['COMPRESS=NONE', 'BLOCKSIZE=10'],
                                outputType = dt)
        out_ds.BuildOverviews('AVG', [2])
        out_ds = None

        expected_cs = 1152

        ds = gdal.Open('/vsimem/out.mrf')
        cs= ds.GetRasterBand(1).GetOverview(0).Checksum()
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(dt)
            print(cs)
            print(expected_cs)
            return 'fail'
        ds = None

        gdal.Unlink('/vsimem/out.mrf')
        gdal.Unlink('/vsimem/out.mrf.aux.xml')
        gdal.Unlink('/vsimem/out.idx')
        gdal.Unlink('/vsimem/out.ppg')
        gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_overview_avg_with_nodata_fact_2():

    for dt in [ gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
                gdal.GDT_Int32, gdal.GDT_UInt32,
                gdal.GDT_Float32, gdal.GDT_Float64 ]:

        out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                                format = 'MRF',
                                creationOptions = ['COMPRESS=NONE', 'BLOCKSIZE=10'],
                                outputType = dt,
                                noData = 107)
        out_ds.BuildOverviews('AVG', [2])
        out_ds = None

        expected_cs = 1164

        ds = gdal.Open('/vsimem/out.mrf')
        cs= ds.GetRasterBand(1).GetOverview(0).Checksum()
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(dt)
            print(cs)
            print(expected_cs)
            return 'fail'
        ds = None

        gdal.Unlink('/vsimem/out.mrf')
        gdal.Unlink('/vsimem/out.mrf.aux.xml')
        gdal.Unlink('/vsimem/out.idx')
        gdal.Unlink('/vsimem/out.ppg')
        gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_overview_near_fact_3():

    out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                            format = 'MRF',
                            creationOptions = ['COMPRESS=NONE', 'BLOCKSIZE=10'])
    out_ds.BuildOverviews('NEAR', [3])
    out_ds = None

    ds = gdal.Open('/vsimem/out.mrf')
    cs= ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 478
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_overview_avg_fact_3():

    out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                            format = 'MRF',
                            creationOptions = ['COMPRESS=NONE', 'BLOCKSIZE=10'])
    out_ds.BuildOverviews('AVG', [3])
    out_ds = None

    ds = gdal.Open('/vsimem/out.mrf')
    cs= ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 658
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_overview_avg_with_nodata_fact_3():

    for dt in [ gdal.GDT_Byte, gdal.GDT_Int16, gdal.GDT_UInt16,
                gdal.GDT_Int32, gdal.GDT_UInt32,
                gdal.GDT_Float32, gdal.GDT_Float64 ]:

        out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif',
                                format = 'MRF',
                                creationOptions = ['COMPRESS=NONE', 'BLOCKSIZE=10'],
                                outputType = dt,
                                noData = 107)
        out_ds.BuildOverviews('AVG', [3])
        out_ds = None

        expected_cs = 531

        ds = gdal.Open('/vsimem/out.mrf')
        cs= ds.GetRasterBand(1).GetOverview(0).Checksum()
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(dt)
            print(cs)
            print(expected_cs)
            return 'fail'
        ds = None

        gdal.Unlink('/vsimem/out.mrf')
        gdal.Unlink('/vsimem/out.mrf.aux.xml')
        gdal.Unlink('/vsimem/out.idx')
        gdal.Unlink('/vsimem/out.ppg')
        gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_overview_partial_block():

    out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format = 'MRF',
                            creationOptions = [ 'COMPRESS=NONE', 'BLOCKSIZE=8' ] )
    out_ds.BuildOverviews('NEAR', [2])
    out_ds = None

    ds = gdal.Open('/vsimem/out.mrf')
    cs= ds.GetRasterBand(1).GetOverview(0).Checksum()
    if cs != 1087:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_overview_near_implicit_level():

    # We ask only overview level 2, but MRF automatically creates 2 and 4
    # so check that 4 is properly initialized
    out_ds = gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format = 'MRF',
                            creationOptions = [ 'COMPRESS=NONE', 'BLOCKSIZE=5' ])
    out_ds.BuildOverviews('NEAR', [2])
    out_ds = None

    ds = gdal.Open('/vsimem/out.mrf')
    cs= ds.GetRasterBand(1).GetOverview(1).Checksum()
    if cs != 328:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    ds = None

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.mrf:MRF:L2')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = gdal.Open('/vsimem/out.mrf:MRF:L1')
    cs= ds.GetRasterBand(1).Checksum()
    if cs != 328:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_overview_external():

    gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format = 'MRF')
    ds = gdal.Open('/vsimem/out.mrf')
    ds.BuildOverviews('NEAR', [2])
    ds = None

    ds = gdal.Open('/vsimem/out.mrf')
    cs= ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 1087
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    return 'success'


def mrf_lerc_nodata():

    gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format = 'MRF',
                   noData = 107, creationOptions = ['COMPRESS=LERC'])
    ds = gdal.Open('/vsimem/out.mrf')
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    if nodata != 107:
        gdaltest.post_reason('fail')
        print(nodata)
        return 'fail'
    cs= ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_lerc_with_huffman():

    gdal.Translate('/vsimem/out.mrf', 'data/small_world.tif', format = 'MRF',
                   width = 5000, height = 5000, creationOptions = ['COMPRESS=LERC'])
    ds = gdal.Open('/vsimem/out.mrf')
    cs= ds.GetRasterBand(1).Checksum()
    expected_cs = 31204
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_cached_source():

    # Caching MRF
    gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format = 'MRF',
                   creationOptions = ['CACHEDSOURCE=invalid_source', 'NOCOPY=TRUE'])
    ds = gdal.Open('/vsimem/out.mrf')
    with gdaltest.error_handler():
        cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 0
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    gdal.Unlink('tmp/byte.idx')
    gdal.Unlink('tmp/byte.ppg')

    open('tmp/byte.tif', 'wb').write(open('data/byte.tif', 'rb').read())
    gdal.Translate('tmp/out.mrf', 'tmp/byte.tif', format = 'MRF',
                   creationOptions = ['CACHEDSOURCE=byte.tif', 'NOCOPY=TRUE'])
    ds = gdal.Open('tmp/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    gdal.Unlink('tmp/byte.tif')
    ds = gdal.Open('tmp/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None


    # Caching MRF in mp_safe mode

    gdal.Unlink('tmp/out.mrf')
    gdal.Unlink('tmp/out.mrf.aux.xml')
    gdal.Unlink('tmp/out.idx')
    gdal.Unlink('tmp/out.ppg')
    gdal.Unlink('tmp/out.til')

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
  <GeoTags>
    <BoundingBox minx="440720.00000000" miny="3750120.00000000" maxx="441920.00000000" maxy="3751320.00000000" />
    <Projection>PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982138982,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]]</Projection>
  </GeoTags>
</MRF_META>""")
    ds = gdal.Open('tmp/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    gdal.Unlink('tmp/byte.tif')
    ds = gdal.Open('tmp/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None


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
  <GeoTags>
    <BoundingBox minx="440720.00000000" miny="3750120.00000000" maxx="441920.00000000" maxy="3751320.00000000" />
    <Projection>PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982138982,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]]</Projection>
  </GeoTags>
</MRF_META>""")
    ds = gdal.Open('tmp/cloning.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    gdal.Unlink('tmp/out.mrf')
    gdal.Unlink('tmp/out.mrf.aux.xml')
    gdal.Unlink('tmp/out.idx')
    gdal.Unlink('tmp/out.ppg')
    gdal.Unlink('tmp/out.til')

    ds = gdal.Open('tmp/cloning.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    gdal.Unlink('tmp/cloning.mrf')
    gdal.Unlink('tmp/cloning.mrf.aux.xml')
    gdal.Unlink('tmp/cloning.idx')
    gdal.Unlink('tmp/cloning.ppg')
    gdal.Unlink('tmp/cloning.til')

    return 'success'

def mrf_versioned():

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    # Caching MRF
    gdal.Translate('/vsimem/out.mrf', 'data/byte.tif', format = 'MRF' )
    gdal.FileFromMemBuffer('/vsimem/out.mrf',
"""<MRF_META>
  <Raster versioned="on">
    <Size x="20" y="20" c="1" />
    <PageSize x="512" y="512" c="1" />
  </Raster>
  <GeoTags>
    <BoundingBox minx="440720.00000000" miny="3750120.00000000" maxx="441920.00000000" maxy="3751320.00000000" />
    <Projection>PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982138982,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]]</Projection>
  </GeoTags>
</MRF_META>""")
    ds = gdal.Open('/vsimem/out.mrf', gdal.GA_Update)
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = gdal.Open('/vsimem/out.mrf')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 0
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    ds = gdal.Open('/vsimem/out.mrf:MRF:V0')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 0
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    ds = gdal.Open('/vsimem/out.mrf:MRF:V1')
    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 4672
    if cs != expected_cs:
        gdaltest.post_reason('fail')
        print(cs)
        print(expected_cs)
        return 'fail'
    ds = None

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/out.mrf:MRF:V2')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    return 'success'

def mrf_cleanup():

    files = [
'12bit_rose_extract.jpg.idx',
'12bit_rose_extract.jpg.pjg',
'12bit_rose_extract.jpg.tst',
'12bit_rose_extract.jpg.tst.aux.xml',
'byte.tif.idx',
'byte.tif.lrc',
'byte.tif.pjg',
'byte.tif.ppg',
'byte.tif.pzp',
'byte.tif.til',
'byte.tif.tst',
'byte.tif.tst.aux.xml',
'int16.tif.idx',
'int16.tif.lrc',
'int16.tif.ppg',
'int16.tif.tst',
'int16.tif.tst.aux.xml',
'out.idx',
'out.mrf',
'out.mrf.aux.xml',
'out.ppg',
'rgbsmall.tif.idx',
'rgbsmall.tif.pjg',
'rgbsmall.tif.tst',
'rgbsmall.tif.tst.aux.xml',
'small_world_pct.tif.idx',
'small_world_pct.tif.ppg',
'small_world_pct.tif.tst',
'small_world_pct.tif.tst.aux.xml',
'cloning.idx',
'cloning.mrf',
'cloning.mrf.aux.xml',
'cloning.ppg' ]

    for f in files:
        gdal.Unlink('tmp/' + f)

    gdal.Unlink('/vsimem/out.mrf')
    gdal.Unlink('/vsimem/out.mrf.aux.xml')
    gdal.Unlink('/vsimem/out.idx')
    gdal.Unlink('/vsimem/out.ppg')
    gdal.Unlink('/vsimem/out.til')

    return 'success'

gdaltest_list = []


class myTestCreateCopyWrapper:

    def __init__(self, ut):
        self.ut = ut

    def myTestCreateCopy(self):
        check_minmax = not 'COMPRESS=JPEG' in self.ut.options
        for x in self.ut.options:
            if x.find('OPTIONS:LERC_PREC=') >= 0:
                check_minmax = False
        return self.ut.testCreateCopy(check_minmax = check_minmax)

for item in init_list:
    src_filename = item[0]
    with gdaltest.error_handler():
        ds = gdal.Open('data/' + src_filename)
    if ds is None:
        continue
    ds = None
    options = []
    if item[3]:
        options = item[3]
    chksum_param = item[2]
    if type(chksum_param) == type([]):
        chksum = chksum_param[0]
        chksum_after_reopening = chksum_param[1]
    else:
        chksum = chksum_param
        chksum_after_reopening = chksum_param
    ut = gdaltest.GDALTest( 'MRF', src_filename, item[1], chksum, options = options, chksum_after_reopening = chksum_after_reopening )
    if ut is None:
        print( 'MRF tests skipped' )
    ut = myTestCreateCopyWrapper(ut)
    gdaltest_list.append( (ut.myTestCreateCopy, item[0] + ' ' + str(options)) )

gdaltest_list += [ mrf_overview_near_fact_2 ]
gdaltest_list += [ mrf_overview_near_with_nodata_fact_2 ]
gdaltest_list += [ mrf_overview_avg_fact_2 ]
gdaltest_list += [ mrf_overview_avg_with_nodata_fact_2 ]
gdaltest_list += [ mrf_overview_near_fact_3 ]
gdaltest_list += [ mrf_overview_avg_fact_3 ]
gdaltest_list += [ mrf_overview_avg_with_nodata_fact_3 ]
gdaltest_list += [ mrf_overview_partial_block ]
gdaltest_list += [ mrf_overview_near_implicit_level ]
gdaltest_list += [ mrf_overview_external ]
gdaltest_list += [ mrf_lerc_nodata ]
gdaltest_list += [ mrf_lerc_with_huffman ]
gdaltest_list += [ mrf_cached_source ]
gdaltest_list += [ mrf_versioned ]
gdaltest_list += [ mrf_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'mrf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
