#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic integration with Numeric Python.
# Author:   Frank Warmerdam, warmerdam@pobox.com
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

###############################################################################
# verify that we can load Numeric python, and find the Numpy driver.

def numpy_rw_1():
	
    gdaltest.numpy_drv = None
    try:
        from osgeo import gdalnumeric
        gdalnumeric.zeros
    except:
        try:
            import osgeo.gdal_array as gdalnumeric
        except ImportError:
            return 'skip'

    try:
        import _gdal
        _gdal.GDALRegister_NUMPY()  # only needed for old style bindings.
        gdal.AllRegister()
    except:
        pass

    gdaltest.numpy_drv = gdal.GetDriverByName( 'NUMPY' )
    if gdaltest.numpy_drv is None:
        gdaltest.post_reason( 'NUMPY driver not found!' )
        return 'fail'
    
    return 'success'

###############################################################################
# Load a test file into a memory Numpy array, and verify the checksum.

def numpy_rw_2():

    if gdaltest.numpy_drv is None:
        return 'skip'

    from osgeo import gdalnumeric

    array = gdalnumeric.LoadFile( 'data/utmsmall.tif' )
    if array is None:
        gdaltest.post_reason( 'Failed to load utmsmall.tif into array')
        return 'fail'

    ds = gdalnumeric.OpenArray( array )
    if ds is None:
        gdaltest.post_reason( 'Failed to open memory array as dataset.' )
        return 'fail'

    bnd = ds.GetRasterBand(1)
    if bnd.Checksum() != 50054:
        gdaltest.post_reason( 'Didnt get expected checksum on reopened file')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test loading complex data.

def numpy_rw_3():

    if gdaltest.numpy_drv is None:
        return 'skip'

    from osgeo import gdalnumeric

    ds = gdal.Open( 'data/cint_sar.tif' )
    array = ds.ReadAsArray()

    if array[2][3] != 116-16j:
        print(array[0][2][3])
        gdaltest.post_reason( 'complex value read improperly.' )
        return 'fail'

    return 'success'

###############################################################################
# Test a band read with downsampling.

def numpy_rw_4():

    if gdaltest.numpy_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/byte.tif' )
    array = ds.GetRasterBand(1).ReadAsArray(0,0,20,20,5,5)

    if array[2][3] != 123:
        print(array[2][3])
        gdaltest.post_reason( 'Read wrong value - perhaps downsampling algorithm has changed subtly?' )
        return 'fail'

    return 'success'

###############################################################################
# Test reading a multi-band file.

def numpy_rw_5():

    if gdaltest.numpy_drv is None:
        return 'skip'

    from osgeo import gdalnumeric

    array = gdalnumeric.LoadFile('data/rgbsmall.tif',35,21,1,1)

    if array[0][0][0] != 78:
        print(array)
        gdaltest.post_reason( 'value read improperly.' )
        return 'fail'

    if array[1][0][0] != 117:
        print(array)
        gdaltest.post_reason( 'value read improperly.' )
        return 'fail'

    if array[2][0][0] != 24:
        print(array)
        gdaltest.post_reason( 'value read improperly.' )
        return 'fail'

    return 'success'

###############################################################################
# Check that Band.ReadAsArray() can accept an already allocated array (#2658, #3028)

def numpy_rw_6():

    if gdaltest.numpy_drv is None:
        return 'skip'

    import numpy
    from osgeo import gdalnumeric
        
    ds = gdal.Open( 'data/byte.tif' )
    array = numpy.zeros( [ds.RasterYSize, ds.RasterXSize], numpy.uint8 )
    array_res = ds.GetRasterBand(1).ReadAsArray(buf_obj = array)
    
    if array is not array_res:
        return 'fail'

    ds2 = gdalnumeric.OpenArray( array )
    if ds2.GetRasterBand(1).Checksum() != ds.GetRasterBand(1).Checksum():
        return 'fail'

    return 'success'

###############################################################################
# Check that Dataset.ReadAsArray() can accept an already allocated array (#2658, #3028)

def numpy_rw_7():

    if gdaltest.numpy_drv is None:
        return 'skip'

    import numpy
    from osgeo import gdalnumeric
        
    ds = gdal.Open( 'data/byte.tif' )
    array = numpy.zeros( [1, ds.RasterYSize, ds.RasterXSize], numpy.uint8 )
    array_res = ds.ReadAsArray(buf_obj = array)
    
    if array is not array_res:
        return 'fail'

    ds2 = gdalnumeric.OpenArray( array )
    if ds2.GetRasterBand(1).Checksum() != ds.GetRasterBand(1).Checksum():
        return 'fail'
        
    # Try again with a 2D array
    array = numpy.zeros( [ds.RasterYSize, ds.RasterXSize], numpy.uint8 )
    array_res = ds.ReadAsArray(buf_obj = array)
    
    if array is not array_res:
        return 'fail'

    ds2 = gdalnumeric.OpenArray( array )
    if ds2.GetRasterBand(1).Checksum() != ds.GetRasterBand(1).Checksum():
        return 'fail'
        
    return 'success'
    
###############################################################################
# Check that Dataset.ReadAsArray() with multi-band data

def numpy_rw_8():

    if gdaltest.numpy_drv is None:
        return 'skip'

    import numpy
    from osgeo import gdalnumeric
        
    ds = gdal.Open( 'data/rgbsmall.tif' )
    array = numpy.zeros( [ds.RasterCount,ds.RasterYSize, ds.RasterXSize], numpy.uint8 )
    array_res = ds.ReadAsArray(buf_obj = array)

    ds2 = gdalnumeric.OpenArray( array )
    for i in range(1, ds.RasterCount):
        if ds2.GetRasterBand(i).Checksum() != ds.GetRasterBand(i).Checksum():
            return 'fail'
        
    return 'success'
    
###############################################################################
# Test Band.WriteArray()

def numpy_rw_9():

    if gdaltest.numpy_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/byte.tif' )
    array = ds.ReadAsArray()

    out_ds = gdal.GetDriverByName('MEM').Create('', ds.RasterYSize, ds.RasterXSize)
    out_ds.GetRasterBand(1).WriteArray(array)
    cs = out_ds.GetRasterBand(1).Checksum()
    out_ds = None
    ds = None

    if cs != 4672:
        gdaltest.post_reason('did not get expected checksum')
        print(cs)
        return 'fail'

    return 'success'
    
###############################################################################
# Test signed byte handling

def numpy_rw_10():

    if gdaltest.numpy_drv is None:
        return 'skip'

    import numpy

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/signed8.tif', 2, 1, options = ['PIXELTYPE=SIGNEDBYTE'])
    ar = numpy.empty([1, 2], dtype = numpy.int8)
    ar[0][0] = -128
    ar[0][1] = 127
    ds.GetRasterBand(1).WriteArray(ar)
    ds = None

    ds = gdal.Open('/vsimem/signed8.tif')
    ar2 = ds.ReadAsArray()
    ar3 = numpy.empty_like(ar2)
    ds.GetRasterBand(1).ReadAsArray(buf_obj = ar3)
    ds = None

    gdal.Unlink('/vsimem/signed8.tif')

    if ar2[0][0] != -128 or ar2[0][1] != 127:
        gdaltest.post_reason('did not get expected result (1)')
        print(ar2)
        return 'fail'

    if ar3[0][0] != -128 or ar3[0][1] != 127:
        gdaltest.post_reason('did not get expected result (2)')
        print(ar3)
        return 'fail'

    return 'success'

###############################################################################
# Test all datatypes

def numpy_rw_11():

    if gdaltest.numpy_drv is None:
        return 'skip'

    import numpy

    type_tuples = [ ( 'uint8', gdal.GDT_Byte, numpy.uint8, 255 ),
                    ( 'uint16', gdal.GDT_UInt16, numpy.uint16, 65535 ),
                    ( 'int16', gdal.GDT_Int16, numpy.int16, -32767 ),
                    ( 'uint32', gdal.GDT_UInt32, numpy.uint32, 4294967295 ),
                    ( 'int32', gdal.GDT_Int32, numpy.int32, -2147483648 ),
                    ( 'float32', gdal.GDT_Float32, numpy.float32, 1.23 ),
                    ( 'float64', gdal.GDT_Float64, numpy.float64, 1.23456789 ),
                    ( 'cint16', gdal.GDT_CInt16, numpy.complex64, -32768 + 32767j ),
                    ( 'cint32', gdal.GDT_CInt32, numpy.complex64, -32769 + 32768j ),
                    ( 'cfloat32', gdal.GDT_CFloat32, numpy.complex64, -32768.5 + 32767.5j ),
                    ( 'cfloat64', gdal.GDT_CFloat64, numpy.complex128, -32768.123456 + 32767.123456j ) ]

    for type_tuple in type_tuples:
        ds = gdal.GetDriverByName('GTiff').Create('/vsimem/' + type_tuple[0], 1, 1, 1, type_tuple[1])
        tmp = ds.ReadAsArray()
        if tmp.dtype != type_tuple[2]:
            gdaltest.post_reason('did not get expected numpy type')
            print(type_tuple)
            return 'fail'

        ar = numpy.empty([1, 1], dtype = type_tuple[2])
        ar[0][0] = type_tuple[3]
        ds.GetRasterBand(1).WriteArray(ar)
        ds = None

        ds = gdal.Open('/vsimem/' + type_tuple[0])
        ar2 = ds.ReadAsArray()
        ar3 = numpy.empty_like(ar2)
        ds.GetRasterBand(1).ReadAsArray(buf_obj = ar3)
        ds = None

        gdal.Unlink('/vsimem/' + type_tuple[0])

        if (type_tuple[0] == 'float32' and abs(ar2[0][0] - type_tuple[3]) > 1e-6) or \
           (type_tuple[0] != 'float32' and ar2[0][0] != type_tuple[3]):
            gdaltest.post_reason('did not get expected result (1)')
            print(ar2)
            print(type_tuple)
            return 'fail'

        if (type_tuple[0] == 'float32' and abs(ar3[0][0] - type_tuple[3]) > 1e-6) or \
           (type_tuple[0] != 'float32' and ar3[0][0] != type_tuple[3]):
            gdaltest.post_reason('did not get expected result (2)')
            print(ar3)
            print(type_tuple)
            return 'fail'

    return 'success'

###############################################################################
# Test array with slices (#3542)

def numpy_rw_12():

    if gdaltest.numpy_drv is None:
        return 'skip'

    import numpy

    ar = numpy.empty([2, 2], dtype = numpy.uint8)
    ar[0][0] = 0
    ar[0][1] = 1
    ar[1][0] = 2
    ar[1][1] = 3

    drv = gdal.GetDriverByName( 'MEM' )
    ds = drv.Create( '', 1, 2, 1, gdal.GDT_Byte )
    slice = ar[:,1:]

    ds.GetRasterBand(1).WriteArray( slice )

    ar_read = numpy.zeros_like(ar)
    slice_read = ar_read[:,1:]
    ds.GetRasterBand(1).ReadAsArray( buf_obj = slice_read )
    ds = None

    if slice_read[0][0] != 1 or slice_read[1][0] != 3:
        print(slice_read)
        return 'fail'

    return 'success'

###############################################################################
# Test expected errors

def numpy_rw_13():

    if gdaltest.numpy_drv is None:
        return 'skip'

    import numpy

    drv = gdal.GetDriverByName( 'MEM' )
    ds = drv.Create( '', 2, 1, 1, gdal.GDT_Byte )
    ar = numpy.empty([1, 2], dtype = numpy.uint8)
    ar[0][0] = 100
    ar[0][1] = 200
    ds.GetRasterBand(1).WriteArray( ar )

    # Try reading into unsupported array type
    ar = numpy.empty([1, 2], dtype = numpy.int64)
    try:
        ds.GetRasterBand(1).ReadAsArray( buf_obj = ar )
        gdaltest.post_reason('expected "ValueError: array does not have corresponding GDAL data type"')
        return 'fail'
    except:
        pass

    # Try call with inconsistant parameters
    ar = numpy.empty([1, 2], dtype = numpy.uint8)
    try:
        ds.GetRasterBand(1).ReadAsArray( buf_obj = ar, buf_xsize = 2, buf_ysize = 2 )
        gdaltest.post_reason('expected "Specified buf_ysize not consistant with buffer shape"')
        return 'fail'
    except:
        pass

    # Try call with inconsistant parameters
    ar = numpy.empty([1, 2], dtype = numpy.uint8)
    try:
        ds.GetRasterBand(1).ReadAsArray( buf_obj = ar, buf_xsize = 1, buf_ysize = 1 )
        gdaltest.post_reason('expected "Specified buf_xsize not consistant with buffer shape"')
        return 'fail'
    except:
        pass

    # This one should be OK !
    ar = numpy.zeros([1, 2], dtype = numpy.uint8)
    ds.GetRasterBand(1).ReadAsArray( buf_obj = ar, buf_xsize = 2, buf_ysize = 1 )
    if ar[0][0] != 100 or ar[0][1] != 200:
        gdaltest.post_reason('did not get expected values')
        print(ar)
        return 'fail'

    ds = None

    return 'success'

def numpy_rw_cleanup():
    gdaltest.numpy_drv = None

    return 'success'

gdaltest_list = [
    numpy_rw_1,
    numpy_rw_2,
    numpy_rw_3,
    numpy_rw_4,
    numpy_rw_5,
    numpy_rw_6,
    numpy_rw_7,
    numpy_rw_8,
    numpy_rw_9,
    numpy_rw_10,
    numpy_rw_11,
    numpy_rw_12,
    numpy_rw_13,
    numpy_rw_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'numpy_rw' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

