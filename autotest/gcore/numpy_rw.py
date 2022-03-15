#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic integration with numpy.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
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

import gdaltest
from osgeo import gdal
import pytest


# All tests will be skipped if numpy or gdal_array are unavailable.
numpy = pytest.importorskip('numpy')
gdal_array = pytest.importorskip('osgeo.gdal_array')

###############################################################################
# verify that we can load the deprecated gdalnumeric module


def test_numpy_rw_1():

    from osgeo import gdalnumeric
    gdalnumeric.zeros

###############################################################################
# Load a test file into a memory Numpy array, and verify the checksum.


def test_numpy_rw_2():

    array = gdal_array.LoadFile('data/utmsmall.tif')
    assert array is not None, 'Failed to load utmsmall.tif into array'

    ds = gdal_array.OpenArray(array)
    assert ds is not None, 'Failed to open memory array as dataset.'

    bnd = ds.GetRasterBand(1)
    assert bnd.Checksum() == 50054, 'Didnt get expected checksum on reopened file'
    ds = None

###############################################################################
# Test loading complex data.


def test_numpy_rw_3():

    ds = gdal.Open('data/cint_sar.tif')
    array = ds.ReadAsArray()

    assert array[2][3] == 116 - 16j, 'complex value read improperly.'

###############################################################################
# Test a band read with downsampling.


def test_numpy_rw_4():

    ds = gdal.Open('data/byte.tif')
    array = ds.GetRasterBand(1).ReadAsArray(0, 0, 20, 20, 5, 5)

    assert array[2][3] == 123, \
        'Read wrong value - perhaps downsampling algorithm has changed subtly?'

###############################################################################
# Test reading a multi-band file.


def test_numpy_rw_5():

    array = gdal_array.LoadFile('data/rgbsmall.tif', 35, 21, 1, 1)

    assert array[0][0][0] == 78, 'value read improperly.'

    assert array[1][0][0] == 117, 'value read improperly.'

    assert array[2][0][0] == 24, 'value read improperly.'

    array = gdal_array.LoadFile('data/rgbsmall.tif', buf_xsize=1, buf_ysize=1, resample_alg=gdal.GRIORA_Bilinear)
    assert array.shape[0] == 3 and array.shape[1] == 1 and array.shape[2] == 1, \
        'wrong array shape.'
    assert array[0][0][0] == 70 and array[1][0][0] == 97 and array[2][0][0] == 29, \
        'value read improperly.'

    array = numpy.zeros([3, 1, 1], dtype=numpy.uint8)
    ds = gdal.Open('data/rgbsmall.tif')
    ds.ReadAsArray(buf_obj=array, resample_alg=gdal.GRIORA_Bilinear)
    assert array[0][0][0] == 70 and array[1][0][0] == 97 and array[2][0][0] == 29, \
        'value read improperly.'

###############################################################################
# Check that Band.ReadAsArray() can accept an already allocated array (#2658, #3028)


def test_numpy_rw_6():

    ds = gdal.Open('data/byte.tif')
    array = numpy.zeros([ds.RasterYSize, ds.RasterXSize], numpy.uint8)
    array_res = ds.GetRasterBand(1).ReadAsArray(buf_obj=array)

    assert array is array_res

    ds2 = gdal_array.OpenArray(array)
    assert ds2.GetRasterBand(1).Checksum() == ds.GetRasterBand(1).Checksum()

###############################################################################
# Check that Dataset.ReadAsArray() can accept an already allocated array (#2658, #3028)


def test_numpy_rw_7():

    ds = gdal.Open('data/byte.tif')
    array = numpy.zeros([1, ds.RasterYSize, ds.RasterXSize], numpy.uint8)
    array_res = ds.ReadAsArray(buf_obj=array)

    assert array is array_res

    ds2 = gdal_array.OpenArray(array)
    assert ds2.GetRasterBand(1).Checksum() == ds.GetRasterBand(1).Checksum()

    # Try again with a 2D array
    array = numpy.zeros([ds.RasterYSize, ds.RasterXSize], numpy.uint8)
    array_res = ds.ReadAsArray(buf_obj=array)

    assert array is array_res

    ds2 = gdal_array.OpenArray(array)
    assert ds2.GetRasterBand(1).Checksum() == ds.GetRasterBand(1).Checksum()

    # With a multi band file
    ds = gdal.Open('data/rgbsmall.tif')
    array = numpy.zeros([ds.RasterCount, ds.RasterYSize, ds.RasterXSize], numpy.uint8)
    array_res = ds.ReadAsArray(buf_obj=array)

    assert array is array_res

    ds2 = gdal_array.OpenArray(array)
    assert ds2.GetRasterBand(1).Checksum() == ds.GetRasterBand(1).Checksum()

###############################################################################
# Check that Dataset.ReadAsArray() with multi-band data


def test_numpy_rw_8():

    ds = gdal.Open('data/rgbsmall.tif')
    array = numpy.zeros([ds.RasterCount, ds.RasterYSize, ds.RasterXSize], numpy.uint8)
    ds.ReadAsArray(buf_obj=array)

    ds2 = gdal_array.OpenArray(array)
    for i in range(1, ds.RasterCount):
        assert ds2.GetRasterBand(i).Checksum() == ds.GetRasterBand(i).Checksum()


###############################################################################
# Test Band.WriteArray()


def test_numpy_rw_9():

    ds = gdal.Open('data/byte.tif')
    array = ds.ReadAsArray()

    out_ds = gdal.GetDriverByName('MEM').Create('', ds.RasterYSize, ds.RasterXSize)
    out_ds.GetRasterBand(1).WriteArray(array)
    cs = out_ds.GetRasterBand(1).Checksum()
    out_ds = None
    ds = None

    assert cs == 4672, 'did not get expected checksum'

###############################################################################
# Test signed byte handling


def test_numpy_rw_10():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/signed8.tif', 2, 1, options=['PIXELTYPE=SIGNEDBYTE'])
    ar = numpy.array([[-128, 127]], dtype=numpy.int8)
    ds.GetRasterBand(1).WriteArray(ar)
    ds = None

    ds = gdal.Open('/vsimem/signed8.tif')
    ar2 = ds.ReadAsArray()
    ar3 = numpy.empty_like(ar2)
    ds.GetRasterBand(1).ReadAsArray(buf_obj=ar3)
    ds = None

    gdal.Unlink('/vsimem/signed8.tif')

    assert ar2[0][0] == -128 and ar2[0][1] == 127, 'did not get expected result (1)'

    assert ar3[0][0] == -128 and ar3[0][1] == 127, 'did not get expected result (2)'

###############################################################################
# Test signed byte handling with all values set to nodata


@pytest.mark.parametrize("options", [[], ['SPARSE_OK=YES']])
def test_numpy_rw_10_bis(options):
    """Reproduce https://github.com/mapbox/rasterio/issues/2180"""
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/signed8.tif', 2, 1,
                                              options=['PIXELTYPE=SIGNEDBYTE']+options)
    ar = numpy.array([[-1, -1]], dtype=numpy.int8)
    ds.GetRasterBand(1).WriteArray(ar)
    ds.GetRasterBand(1).SetNoDataValue(-1)
    ds = None

    ds = gdal.Open('/vsimem/signed8.tif')
    ar2 = ds.ReadAsArray()
    ds = None
    gdal.Unlink('/vsimem/signed8.tif')

    assert ar2[0][0] == -1 and ar2[0][1] == -1

###############################################################################
# Test all datatypes


def test_numpy_rw_11():

    type_tuples = [('uint8', gdal.GDT_Byte, numpy.uint8, 255),
                   ('uint16', gdal.GDT_UInt16, numpy.uint16, 65535),
                   ('int16', gdal.GDT_Int16, numpy.int16, -32767),
                   ('uint32', gdal.GDT_UInt32, numpy.uint32, 4294967295),
                   ('int32', gdal.GDT_Int32, numpy.int32, -2147483648),
                   ('uint64', gdal.GDT_UInt64, numpy.uint64, 4294967295 * 1000),
                   ('int64', gdal.GDT_Int64, numpy.int64, -2147483648 * 1000),
                   ('float32', gdal.GDT_Float32, numpy.float32, 1.23),
                   ('float64', gdal.GDT_Float64, numpy.float64, 1.23456789),
                   ('cint16', gdal.GDT_CInt16, numpy.complex64, -32768 + 32767j),
                   ('cint32', gdal.GDT_CInt32, numpy.complex64, -32769 + 32768j),
                   ('cfloat32', gdal.GDT_CFloat32, numpy.complex64, -32768.5 + 32767.5j),
                   ('cfloat64', gdal.GDT_CFloat64, numpy.complex128, -32768.123456 + 32767.123456j)]

    for type_tuple in type_tuples:
        ds = gdal.GetDriverByName('GTiff').Create('/vsimem/' + type_tuple[0], 1, 1, 1, type_tuple[1])
        tmp = ds.ReadAsArray()
        assert tmp.dtype == type_tuple[2], 'did not get expected numpy type'

        ar = numpy.empty([1, 1], dtype=type_tuple[2])

        ar_ds = gdal_array.OpenArray(ar)
        got_dt = ar_ds.GetRasterBand(1).DataType
        ar_ds = None
        expected_dt = type_tuple[1]
        if expected_dt == gdal.GDT_CInt16 or expected_dt == gdal.GDT_CInt32:
            expected_dt = gdal.GDT_CFloat32
        if got_dt != expected_dt:
            print(type_tuple[1])
            pytest.fail('did not get expected result (0)')

        ar[0][0] = type_tuple[3]
        ds.GetRasterBand(1).WriteArray(ar)
        ds = None

        ds = gdal.Open('/vsimem/' + type_tuple[0])
        assert ds.GetRasterBand(1).DataType == type_tuple[1]
        ar2 = ds.ReadAsArray()
        ar3 = numpy.empty_like(ar2)
        ds.GetRasterBand(1).ReadAsArray(buf_obj=ar3)
        ds = None

        gdal.Unlink('/vsimem/' + type_tuple[0])

        assert (not (type_tuple[0] == 'float32' and ar2[0][0] != pytest.approx(type_tuple[3], abs=1e-6)) or \
           (type_tuple[0] != 'float32' and ar2[0][0] != type_tuple[3])), \
            'did not get expected result (1)'

        assert (not (type_tuple[0] == 'float32' and ar3[0][0] != pytest.approx(type_tuple[3], abs=1e-6)) or \
           (type_tuple[0] != 'float32' and ar3[0][0] != type_tuple[3])), \
            'did not get expected result (2)'


###############################################################################
# Test array with slices (#3542)


def test_numpy_rw_12():

    ar = numpy.empty([2, 2], dtype=numpy.uint8)
    ar[0][0] = 0
    ar[0][1] = 1
    ar[1][0] = 2
    ar[1][1] = 3

    drv = gdal.GetDriverByName('MEM')
    ds = drv.Create('', 1, 2, 1, gdal.GDT_Byte)

    ds.GetRasterBand(1).WriteArray(ar[:, 1:])

    ar_read = numpy.zeros_like(ar)
    slice_read = ar_read[:, 1:]
    ds.GetRasterBand(1).ReadAsArray(buf_obj=slice_read)
    ds = None

    assert slice_read[0][0] == 1 and slice_read[1][0] == 3

###############################################################################
# Test expected errors


def test_numpy_rw_13():

    drv = gdal.GetDriverByName('MEM')
    ds = drv.Create('', 2, 1, 1, gdal.GDT_Byte)
    ar = numpy.empty([1, 2], dtype=numpy.uint8)
    ar[0][0] = 100
    ar[0][1] = 200
    ds.GetRasterBand(1).WriteArray(ar)

    # Try reading into unsupported array type
    ar = numpy.empty([1, 2], dtype=numpy.bool_)
    with pytest.raises(Exception, match='array does not have '
                             'corresponding GDAL data type'):
        ds.GetRasterBand(1).ReadAsArray(buf_obj=ar)


    # Try call with inconsistent parameters.
    ar = numpy.empty([1, 2], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Specified buf_ysize not consistent '
                             'with array shape'):
        ds.GetRasterBand(1).ReadAsArray(buf_obj=ar, buf_xsize=2,
                                        buf_ysize=2)


    # Same with 3 dimensions
    ar = numpy.empty([1, 1, 2], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Specified buf_ysize not consistent '
                             'with array shape'):
        ds.GetRasterBand(1).ReadAsArray(buf_obj=ar, buf_xsize=2,
                                        buf_ysize=2)


    # Try call with inconsistent parameters.
    ar = numpy.empty([1, 2], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Specified buf_xsize not consistent '
                             'with array shape'):
        ds.GetRasterBand(1).ReadAsArray(buf_obj=ar, buf_xsize=1,
                                        buf_ysize=1)


    # Inconsistent data type
    ar = numpy.empty([1, 2], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Specified buf_type not consistent '
                             'with array type'):
        ds.GetRasterBand(1).ReadAsArray(buf_obj=ar,
                                        buf_type=gdal.GDT_Int16)


    # This one should be OK !
    ar = numpy.zeros([1, 2], dtype=numpy.uint8)
    ds.GetRasterBand(1).ReadAsArray(buf_obj=ar, buf_xsize=2, buf_ysize=1)
    assert ar[0][0] == 100 and ar[0][1] == 200, 'did not get expected values'

    # This one too
    ar = numpy.zeros([1, 1, 2], dtype=numpy.uint8)
    ds.GetRasterBand(1).ReadAsArray(buf_obj=ar)
    assert ar[0][0][0] == 100 and ar[0][0][1] == 200, 'did not get expected values'

    # This one too
    ar = numpy.zeros([1, 1, 2], dtype=numpy.uint8)
    ds.ReadAsArray(buf_obj=ar)
    assert ar[0][0][0] == 100 and ar[0][0][1] == 200, 'did not get expected values'

    # This one too
    ar = ds.ReadAsArray()
    assert ar[0][0] == 100 and ar[0][1] == 200, 'did not get expected values'

    ds = None

    # With a multiband file
    drv = gdal.GetDriverByName('MEM')
    ds = drv.Create('', 2, 1, 3, gdal.GDT_Byte)
    ar = numpy.empty([3, 1, 2], dtype=numpy.uint8)
    ar[0][0][0] = 100
    ar[0][0][1] = 200
    ar[1][0][0] = 101
    ar[1][0][1] = 201
    ar[2][0][0] = 102
    ar[2][0][1] = 202
    for i in range(3):
        ds.GetRasterBand(i + 1).WriteArray(ar[i])

    ar = numpy.empty([3, 1, 2], dtype=numpy.bool_)
    with pytest.raises(Exception, match='array does not have '
                             'corresponding GDAL data type'):
        ds.ReadAsArray(buf_obj=ar)


    # Try call with inconsistent parameters.
    ar = numpy.empty([3, 1, 2], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Specified buf_ysize not consistent '
                             'with array shape'):
        ds.ReadAsArray(buf_obj=ar, buf_xsize=2, buf_ysize=2)


    # With 2 dimensions
    ar = numpy.empty([1, 2], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Array should have 3 '
                             'dimensions'):
        ds.ReadAsArray(buf_obj=ar)


    # Try call with inconsistent parameters
    ar = numpy.empty([3, 1, 2], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Specified buf_xsize not consistent '
                             'with array shape'):
        ds.ReadAsArray(buf_obj=ar, buf_xsize=1, buf_ysize=1)

    ar = numpy.empty([1, 2, 3], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Specified buf_xsize not consistent '
                             'with array shape'):
        ds.ReadAsArray(buf_obj=ar, buf_xsize=1, buf_ysize=1, interleave='pixel')

    # Inconsistent data type
    ar = numpy.empty([3, 1, 2], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Specified buf_type not consistent with array type'):
        ds.ReadAsArray(buf_obj=ar, buf_type=gdal.GDT_Int16)


    # Not enough space in first dimension
    ar = numpy.empty([2, 1, 2], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Dimension 0 of array should have size 3 to store bands'):
        ds.ReadAsArray(buf_obj=ar)

    # Not enough space in third dimension
    ar = numpy.empty([1, 2, 2], dtype=numpy.uint8)
    with pytest.raises(Exception, match='Dimension 2 of array should have size 3 to store bands'):
        ds.ReadAsArray(buf_obj=ar, interleave='pixel')

    # This one should be OK !
    ar = numpy.zeros([3, 1, 2], dtype=numpy.uint8)
    ds.ReadAsArray(buf_obj=ar, buf_xsize=2, buf_ysize=1, buf_type=gdal.GDT_Byte)
    assert ar[0][0][0] == 100 and ar[0][0][1] == 200 and ar[1][0][0] == 101 and ar[1][0][1] == 201 and ar[2][0][0] == 102 and ar[2][0][1] == 202, \
        'did not get expected values'

    # This one too
    ar = numpy.zeros([3, 1, 2], dtype=numpy.uint8)
    ds.ReadAsArray(buf_obj=ar)
    assert ar[0][0][0] == 100 and ar[0][0][1] == 200 and ar[1][0][0] == 101 and ar[1][0][1] == 201 and ar[2][0][0] == 102 and ar[2][0][1] == 202, \
        'did not get expected values'

    # This one too
    ar = ds.ReadAsArray()
    assert ar[0][0][0] == 100 and ar[0][0][1] == 200 and ar[1][0][0] == 101 and ar[1][0][1] == 201 and ar[2][0][0] == 102 and ar[2][0][1] == 202, \
        'did not get expected values'

    ds = None

###############################################################################
# Test callback of ReadAsArray()


def numpy_rw_14_progress_callback(pct, message, user_data):
    # pylint: disable=unused-argument
    if pct != pytest.approx(user_data[0], abs=1e-5):
        print('Expected %f, got %f' % (user_data[0], pct))
        user_data[1] = False
    user_data[0] = user_data[0] + 0.05
    return 1  # 1 to continue, 0 to stop


def numpy_rw_14_progress_interrupt_callback(pct, message, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    if pct >= 0.5:
        return 0
    return 1  # 1 to continue, 0 to stop


def numpy_rw_14_progress_callback_2(pct, message, user_data):
    # pylint: disable=unused-argument
    if pct < user_data[0]:
        print('Got %f, last pct was %f' % (pct, user_data[0]))
        return 0
    user_data[0] = pct
    return 1  # 1 to continue, 0 to stop


def test_numpy_rw_14():

    # Progress not implemented yet
    if gdal.GetConfigOption('GTIFF_DIRECT_IO') == 'YES' or \
       gdal.GetConfigOption('GTIFF_VIRTUAL_MEM_IO') == 'YES':
        pytest.skip()

    ds = gdal.Open('data/byte.tif')

    # Test RasterBand.ReadAsArray
    tab = [0.05, True]
    data = ds.GetRasterBand(1).ReadAsArray(resample_alg=gdal.GRIORA_NearestNeighbour,
                                           callback=numpy_rw_14_progress_callback,
                                           callback_data=tab)
    assert data is not None
    assert tab[0] == pytest.approx(1.05, abs=1e-5) and tab[1]

    # Test interruption
    tab = [0]
    data = ds.GetRasterBand(1).ReadAsArray(callback=numpy_rw_14_progress_interrupt_callback,
                                           callback_data=tab)
    assert data is None
    assert tab[0] >= 0.50

    # Test Dataset.ReadAsArray
    tab = [0.05, True]
    data = ds.ReadAsArray(resample_alg=gdal.GRIORA_NearestNeighbour,
                          callback=numpy_rw_14_progress_callback,
                          callback_data=tab)
    assert data is not None
    assert tab[0] == pytest.approx(1.05, abs=1e-5) and tab[1]

    # Same with interruption
    tab = [0]
    data = ds.ReadAsArray(callback=numpy_rw_14_progress_interrupt_callback,
                          callback_data=tab)
    assert data is None and tab[0] >= 0.50

    # Test Dataset.ReadAsArray on a multi band file
    ds = None
    ds = gdal.Open('data/rgbsmall.tif')
    last_pct = [0]
    data = ds.ReadAsArray(callback=numpy_rw_14_progress_callback_2,
                          callback_data=last_pct)
    assert not (data is None or last_pct[0] != pytest.approx(1.0, abs=1e-5))

    last_pct = [0]

    # Same but with a provided array
    array = numpy.empty([ds.RasterCount, ds.RasterYSize, ds.RasterXSize], numpy.uint8)
    data = ds.ReadAsArray(buf_obj=array,
                          callback=numpy_rw_14_progress_callback_2,
                          callback_data=last_pct)
    assert not (data is None or last_pct[0] != pytest.approx(1.0, abs=1e-5))

###############################################################################
# Test NumPy GetGeoTransform/SetGeoTransform


def test_numpy_rw_15():

    array = numpy.empty([1, 1, 1], numpy.uint8)
    ds = gdal_array.OpenArray(array)
    gt = ds.GetGeoTransform(can_return_null=True)
    assert gt is None
    ds.SetGeoTransform([1, 2, 3, 4, 5, -6])
    gt = ds.GetGeoTransform()
    assert gt == (1, 2, 3, 4, 5, -6)

###############################################################################
# Test errors of OpenArray()


def test_numpy_rw_16():

    # 1D
    array = numpy.empty([1], numpy.uint8)
    with gdaltest.error_handler():
        ds = gdal_array.OpenArray(array)
    assert ds is None

    # 4D
    array = numpy.empty([1, 1, 1, 1], numpy.uint8)
    with gdaltest.error_handler():
        ds = gdal_array.OpenArray(array)
    assert ds is None

    # Unsupported data type
    array = numpy.empty([1, 1], numpy.float16)
    with gdaltest.error_handler():
        ds = gdal_array.OpenArray(array)
    assert ds is None

###############################################################################
# Test old deprecated way with gdal_array.GetArrayFilename()


def test_numpy_rw_17():

    # Disabled by default
    array = numpy.empty([1, 1], numpy.uint8)
    with gdaltest.error_handler():
        ds = gdal.Open(gdal_array.GetArrayFilename(array))
    assert ds is None

    gdal.SetConfigOption('GDAL_ARRAY_OPEN_BY_FILENAME', 'TRUE')
    ds = gdal.Open(gdal_array.GetArrayFilename(array))
    gdal.SetConfigOption('GDAL_ARRAY_OPEN_BY_FILENAME', None)
    assert ds is not None

    # Invalid value
    with gdaltest.error_handler():
        ds = gdal.Open('NUMPY:::invalid')
    assert ds is None

###############################################################################
# Test the pixel interleave options


def test_numpy_rw_18():

    img = numpy.random.randint(0, 255, size=(256, 200, 3)).astype('uint8')
    ds = gdal_array.OpenArray(img, interleave='pixel')
    assert ds is not None, 'Failed to open memory array as dataset.'

    bnd1 = ds.GetRasterBand(1).ReadAsArray()
    bnd2 = ds.GetRasterBand(2).ReadAsArray()
    bnd3 = ds.GetRasterBand(3).ReadAsArray()

    res = numpy.dstack((bnd1, bnd2, bnd3))
    assert numpy.all(img == res)

    res = ds.ReadAsArray(interleave='pixel')
    assert numpy.all(img == res)

    res = numpy.zeros([256, 200, 3])
    ds.ReadAsArray(buf_obj=res, interleave='pixel')
    assert numpy.all(img == res)

###############################################################################
# The VRT references a non existing TIF file, but using the proxy pool dataset API (#2837)

def test_numpy_rw_failure_in_readasarray():

    ds = gdal.Open('data/idontexist2.vrt')
    assert ds is not None

    exception_raised = False
    with gdaltest.enable_exceptions():
        try:
            ds.ReadAsArray()
        except RuntimeError:
            exception_raised = True
    assert exception_raised

    exception_raised = False
    with gdaltest.enable_exceptions():
        try:
            ds.GetRasterBand(1).ReadAsArray()
        except RuntimeError:
            exception_raised = True
    assert exception_raised


###############################################################################
# Test permission handling


def test_numpy_rw_gdal_array_openarray_permissions():

    # Writeable array
    ar = numpy.zeros([1, 1], dtype=numpy.uint8)
    ds = gdal_array.OpenArray(ar)
    assert ds.GetRasterBand(1).Fill(1) == 0
    assert ds.GetRasterBand(1).Checksum() != 0

    # Non-writeable array
    ar = numpy.zeros([1, 1], dtype=numpy.uint8)
    ar.setflags(write=False)
    ds = gdal_array.OpenArray(ar)
    with gdaltest.error_handler():
        assert ds.GetRasterBand(1).Fill(1) != 0
    assert ds.GetRasterBand(1).Checksum()  == 0

    # Cannot read in non-writeable array
    with gdaltest.error_handler():
        assert ds.ReadAsArray(buf_obj = ar) is None
        assert ds.GetRasterBand(1).ReadAsArray(buf_obj = ar) is None


###############################################################################
# Test ReadAsArray RMS subsampling.

def test_numpy_rw_rms_resemple_alg():

    wrk_ds = gdal.Open('../gdrivers/data/int16.tif')
    assert wrk_ds is not None, 'Failed to open test dataset.'

    res = wrk_ds.ReadAsArray(0, 0,
                             wrk_ds.RasterXSize,
                             wrk_ds.RasterYSize,
                             buf_xsize=wrk_ds.RasterXSize//2,
                             buf_ysize=wrk_ds.RasterYSize//2,
                             resample_alg=gdal.GRIORA_RMS)
    cs = res.sum()
    exp_cs = 12773

    assert cs == exp_cs, 'got wrong rms sum'



###############################################################################
# Test Dataset.ReadAsArray() with band_list

def test_numpy_rw_dataset_read_as_array():

    wrk_ds = gdal.Open('../gdrivers/data/rgbsmall.tif')

    assert numpy.array_equal(wrk_ds.ReadAsArray(band_list=[2]),
                             wrk_ds.GetRasterBand(2).ReadAsArray())

    assert numpy.array_equal(wrk_ds.ReadAsArray(band_list=[2,1]),
                             numpy.stack(
                                 [wrk_ds.GetRasterBand(2).ReadAsArray(),
                                  wrk_ds.GetRasterBand(1).ReadAsArray()]))




###############################################################################
# Test Dataset.WriteArray()

def test_numpy_rw_dataset_writearray():

    # Write 2D array in single-band dataset
    ds = gdal.GetDriverByName('MEM').Create('', 3, 2)
    ar = numpy.array([[0,1,2],[3,4,5]], dtype=numpy.uint8)
    assert ds.WriteArray(ar) == 0
    assert numpy.array_equal(ds.ReadAsArray(), ar)

    # Use WriteRaster interface
    ds = gdal.GetDriverByName('MEM').Create('', 3, 2)
    assert ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, ar.astype(numpy.uint32)) == 0
    assert numpy.array_equal(ds.ReadAsArray(), ar)

    with pytest.raises(Exception):
        ds.WriteArray(None)

    with pytest.raises(Exception):
        ds.WriteArray('not an array')

    # 1D array
    with pytest.raises(Exception):
        ds.WriteArray(numpy.array([0, 1, 2], dtype=numpy.uint8))

    # Too big 2D array in X
    with pytest.raises(Exception):
        ds.WriteArray(numpy.array([[0,1,2,10],[3,4,5,20]], dtype=numpy.uint8))

    # Write 3D array in single-band dataset
    ds = gdal.GetDriverByName('MEM').Create('', 3, 2)
    ar = numpy.array([[[0,1,2],[3,4,5]]], dtype=numpy.uint8)
    assert ds.WriteArray(ar) == 0
    assert numpy.array_equal(ds.ReadAsArray(), ar[0])

    # ar.shape[0] != 1
    with pytest.raises(Exception):
        ds.WriteArray(numpy.array([[[0,1,2],[3,4,5]],[[0,1,2],[3,4,5]]]))

    # Write 3D array in two-band dataset
    ds = gdal.GetDriverByName('MEM').Create('', 3, 2, 2)
    ar = numpy.array([[[0,1,2],[3,4,5]],[[10,11,12],[13,14,15]]], dtype=numpy.uint8)
    assert ds.WriteRaster(0,0,3,2,memoryview(ar)) == 0
    assert numpy.array_equal(ds.ReadAsArray(), ar)

    # Non-native data type
    ds = gdal.GetDriverByName('MEM').Create('', 3, 2, 2)
    assert ds.WriteArray(ar.astype(numpy.int32)) == 0
    assert numpy.array_equal(ds.ReadAsArray(), ar)

    # interleave='pixel'
    ds = gdal.GetDriverByName('MEM').Create('', 3, 2, 2)
    assert ds.WriteArray(numpy.transpose(ar, (1,2,0)), interleave='pixel') == 0
    assert numpy.array_equal(ds.ReadAsArray(), ar)

    # band_list
    ds = gdal.GetDriverByName('MEM').Create('', 3, 2, 2)
    assert ds.WriteArray(ar[::-1,...], band_list=[2,1]) == 0
    assert numpy.array_equal(ds.ReadAsArray(), ar)

    # Use WriteRaster interface
    ds = gdal.GetDriverByName('MEM').Create('', 3, 2, 2)
    assert ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, ar.astype(numpy.uint32)) == 0
    assert numpy.array_equal(ds.ReadAsArray(), ar)

    # 2D array
    with pytest.raises(Exception):
        ds.WriteArray(numpy.array([[0,1,2,0],[3,4,5,0]]))

    # Too big 3D array in X
    with pytest.raises(Exception):
        ds.WriteArray(numpy.array([[[0,1,2,0],[3,4,5,0]],[[10,11,12,0],[13,14,15,0]]]))

    # Too big 3D array in Y
    with pytest.raises(Exception):
        ds.WriteArray(numpy.array([[[0,1,2],[3,4,5],[3,4,5]],[[10,11,12],[13,14,15],[13,14,15]]]))

    # Too big 3D array in band dimension
    with pytest.raises(Exception):
        ds.WriteArray(numpy.array([[[0,1,2],[3,4,5]],[[10,11,12],[13,14,15]],[[10,11,12],[13,14,15]]]))


###############################################################################
# Test Band.ReadAsArray() error cases


def test_numpy_rw_band_read_as_array_error_cases():

    ds = gdal.GetDriverByName('MEM').Create('', 3, 2)
    band = ds.GetRasterBand(1)
    assert band.ReadAsArray(buf_obj = numpy.empty((3,2), dtype=numpy.uint8)) is not None
    assert band.ReadAsArray(buf_obj = numpy.empty((1, 3,2), dtype=numpy.uint8)) is not None

    # 1D
    with pytest.raises(Exception, match='expected array of dimension 2 or 3'):
        band.ReadAsArray(buf_obj = numpy.empty((3,), dtype=numpy.uint8))

    # 3D of wrong size in first dimension
    with pytest.raises(Exception, match='expected size of first dimension should be 0'):
        band.ReadAsArray(buf_obj = numpy.empty((2, 3,2), dtype=numpy.uint8))


###############################################################################
# Test that we can get an error (#5374)

def test_numpy_rw_band_read_as_array_getlasterrormsg():

    ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Float64" band="1" subClass="VRTDerivedRasterBand">
    <Description>Scaling</Description>
    <PixelFunctionType>invalid</PixelFunctionType>
    <SourceTransferType>Float64</SourceTransferType>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">notexisting</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    gdal.ErrorReset()
    with gdaltest.error_handler():
        assert ds.GetRasterBand(1).ReadAsArray() is None
    assert gdal.GetLastErrorMsg() != ''
