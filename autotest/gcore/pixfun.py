#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test pixel functions support.
# Author:   Antonio Valentino <antonio.valentino@tiscali.it>
#
###############################################################################
# Copyright (c) 2010-2014, Antonio Valentino <antonio.valentino@tiscali.it>
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
import numpy
from osgeo import gdal

sys.path.append('../pymod')

import gdaltest

# @TODO: remove in non-plugin configuration
#gdal.SetConfigOption('CPL_DEBUG', 'PIXFUN')
gdal.SetConfigOption('GDAL_DRIVER_PATH', '../..')
gdal.AllRegister()


###############################################################################
# Verify real part extraction from a complex dataset.

def pixfun_real_c():

    filename = 'data/pixfun_real_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == refdata.real):
        return 'fail'

    return 'success'


###############################################################################
# Verify real part extraction from a complex dataset.

def pixfun_real_r():

    filename = 'data/pixfun_real_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == refdata.real):
        return 'fail'

    return 'success'


###############################################################################
# Verify imaginary part extraction from a complex dataset.

def pixfun_imag_c():

    filename = 'data/pixfun_imag_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == refdata.imag):
        return 'fail'

    return 'success'


###############################################################################
# Verify imaginary part extraction from a real dataset.

def pixfun_imag_r():

    filename = 'data/pixfun_imag_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == 0):
        return 'fail'

    return 'success'


###############################################################################
# Verify modulus extraction from a complex (float) dataset.

def pixfun_mod_c():

    filename = 'data/pixfun_mod_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == numpy.abs(refdata)):
        return 'fail'

    return 'success'


###############################################################################
# Verify modulus extraction from a real (integer type) dataset.

def pixfun_mod_r():

    filename = 'data/pixfun_mod_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == numpy.abs(refdata)):
        return 'fail'

    return 'success'


###############################################################################
# Verify phase extraction from a complex dataset.

def pixfun_phase_c():

    filename = 'data/pixfun_phase_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()
    refdata = refdata.astype('complex128')

    #if not numpy.allclose(data, numpy.arctan2(refdata.imag, refdata.real)):
    if not numpy.alltrue(data == numpy.arctan2(refdata.imag, refdata.real)):
        return 'fail'

    return 'success'


###############################################################################
# Verify phase extraction from a real dataset.

def pixfun_phase_r():

    filename = 'data/pixfun_phase_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/pixfun_imag_c.vrt'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == numpy.arctan2(0, refdata)):
        return 'fail'

    return 'success'


###############################################################################
# Verify cmplex conjugare computation on a complex dataset.

def pixfun_conj_c():

    filename = 'data/pixfun_conj_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == numpy.conj(refdata)):
        return 'fail'

    return 'success'


###############################################################################
# Verify cmplex conjugare computation on a real dataset.

def pixfun_conj_r():

    filename = 'data/pixfun_conj_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == numpy.conj(refdata)):
        return 'fail'

    return 'success'


###############################################################################
# Verify the sum of 3 (real) datasets.

def pixfun_sum_r():

    filename = 'data/pixfun_sum_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    refdata = numpy.zeros(data.shape, 'float')
    for reffilename in ('data/uint16.tif', 'data/int32.tif',
                        'data/float32.tif'):
        refds = gdal.Open(reffilename)
        if refds is None:
            gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
            return 'fail'
        refdata += refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == refdata):
        return 'fail'

    return 'success'


###############################################################################
# Verify the sum of 3 (two complex and one real) datasets.

def pixfun_sum_c():

    filename = 'data/pixfun_sum_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    refdata = numpy.zeros(data.shape, 'complex')
    for reffilename in ('data/uint16.tif', 'data/cint_sar.tif',
                        'data/cfloat64.tif'):
        refds = gdal.Open(reffilename)
        if refds is None:
            gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
            return 'fail'
        refdata += refds.GetRasterBand(1).ReadAsArray(0, 0, 5, 6)

    if not numpy.alltrue(data == refdata):
        return 'fail'

    return 'success'


###############################################################################
# Verify the difference of 2 (real) datasets.

def pixfun_diff_r():

    filename = 'data/pixfun_diff_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata1 = refds.GetRasterBand(1).ReadAsArray(0, 0, 5, 6)

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata2 = refds.GetRasterBand(1).ReadAsArray(10, 10, 5, 6)

    if not numpy.alltrue(data == refdata1-refdata2):
        return 'fail'

    return 'success'


###############################################################################
# Verify the difference of 2 (complex) datasets.

def pixfun_diff_c():

    filename = 'data/pixfun_diff_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata1 = refds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cfloat64.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata2 = refds.GetRasterBand(1).ReadAsArray(0, 0, 5, 6)

    if not numpy.alltrue(data == refdata1-refdata2):
        return 'fail'

    return 'success'


###############################################################################
# Verify the product of 3 (real) datasets.

def pixfun_mul_r():

    filename = 'data/pixfun_mul_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    refdata = numpy.ones(data.shape, 'float')
    for reffilename in ('data/uint16.tif', 'data/int32.tif',
                        'data/float32.tif'):
        refds = gdal.Open(reffilename)
        if refds is None:
            gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
            return 'fail'
        refdata *= refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == refdata):
        return 'fail'

    return 'success'


###############################################################################
# Verify the product of 2 (complex) datasets.

def pixfun_mul_c():

    filename = 'data/pixfun_mul_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == refdata*refdata):
        return 'fail'

    return 'success'


###############################################################################
# Verify the product with complex conjugate of a complex datasets.

def pixfun_cmul_c():

    filename = 'data/pixfun_cmul_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == refdata*refdata.conj()):
        return 'fail'

    return 'success'


###############################################################################
# Verify the product with complex conjugate of two real datasets.

def pixfun_cmul_r():

    filename = 'data/pixfun_cmul_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/uint16.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata1 = refds.GetRasterBand(1).ReadAsArray()
    refdata1 = refdata1.astype('float64')

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata2 = refds.GetRasterBand(1).ReadAsArray()
    refdata2 = refdata2.astype('float64')

    if not numpy.alltrue(data == refdata1 * refdata2.conj()):
        return 'fail'

    return 'success'


###############################################################################
# Verify computation of the inverse of a real datasets.

def pixfun_inv_r():

    filename = 'data/pixfun_inv_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/uint16.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()
    refdata = refdata.astype('float64')

    if not numpy.alltrue(data == 1./refdata):
        return 'fail'

    return 'success'


###############################################################################
# Verify computation of the inverse of a complex datasets.

def pixfun_inv_c():

    filename = 'data/pixfun_inv_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()
    refdata = refdata.astype('complex')
    delta = data - 1./refdata
    
    if not numpy.alltrue(abs(delta.real) < 1e-13):
        return 'fail'
    if not numpy.alltrue(abs(delta.imag) < 1e-13):
        return 'fail'

    return 'success'


###############################################################################
# Verify intensity computation of a complex dataset.

def pixfun_intensity_c():

    filename = 'data/pixfun_intensity_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == (refdata*refdata.conj()).real):
        return 'fail'

    return 'success'


###############################################################################
# Verify intensity computation of real dataset.

def pixfun_intensity_r():

    filename = 'data/pixfun_intensity_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == (refdata*refdata.conj()).real):
        return 'fail'

    return 'success'


###############################################################################
# Verify square root computation.

def pixfun_sqrt():

    filename = 'data/pixfun_sqrt.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == numpy.sqrt(refdata)):
        return 'fail'

    return 'success'


###############################################################################
# Verify logarithm computation.

def pixfun_log10():

    filename = 'data/pixfun_log10.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    if not numpy.alltrue(data == numpy.log10(refdata)):
        return 'fail'

    return 'success'


###############################################################################
# Verify conversion from dB to amplitude.

def pixfun_dB2amp():

    filename = 'data/pixfun_dB2amp.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()

    #if not numpy.alltrue(data == 10.**(refdata/20.)):
    if not numpy.allclose(data, 10.**(refdata/20.)):
        return 'fail'

    return 'success'


###############################################################################
# Verify conversion from dB to power.

def pixfun_dB2pow():

    filename = 'data/pixfun_dB2pow.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    if ds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % filename)
        return 'fail'
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    if refds is None:
        gdaltest.post_reason('Unable to open "%s" dataset.' % reffilename)
        return 'fail'
    refdata = refds.GetRasterBand(1).ReadAsArray()
    refdata = refdata.astype('float64')

    #if not numpy.allclose(data, 10.**(refdata/10.)):
    if not numpy.alltrue(data == 10.**(refdata/10.)):
        return 'fail'

    return 'success'


###############################################################################

gdaltest_list = [
    pixfun_real_c,
    pixfun_real_r,
    pixfun_imag_c,
    pixfun_imag_r,
    pixfun_mod_c,
    pixfun_mod_r,
    pixfun_phase_c,
    pixfun_phase_r,
    pixfun_conj_c,
    pixfun_conj_r,
    pixfun_sum_r,
    pixfun_sum_c,
    pixfun_diff_r,
    pixfun_diff_c,
    pixfun_mul_r,
    pixfun_mul_c,
    pixfun_cmul_c,
    pixfun_cmul_r,
    pixfun_inv_r,
    pixfun_inv_c,
    pixfun_intensity_c,
    pixfun_intensity_r,
    pixfun_sqrt,
    pixfun_log10,
    pixfun_dB2amp,
    pixfun_dB2pow,
]


if __name__ == '__main__':
    gdaltest.setup_run('pixfun')
    gdaltest.run_tests(gdaltest_list)
    gdaltest.summarize()
