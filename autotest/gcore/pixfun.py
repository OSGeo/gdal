#!/usr/bin/env pytest
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

import math

import gdaltest
import pytest

from osgeo import gdal

# All tests will be skipped if numpy is unavailable.
numpy = pytest.importorskip('numpy')


###############################################################################
# Verify real part extraction from a complex dataset.

def test_pixfun_real_c():

    filename = 'data/pixfun_real_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == refdata.real)


###############################################################################
# Verify real part extraction from a complex dataset.

def test_pixfun_real_r():

    filename = 'data/pixfun_real_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == refdata.real)


###############################################################################
# Verify imaginary part extraction from a complex dataset.

def test_pixfun_imag_c():

    filename = 'data/pixfun_imag_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == refdata.imag)

    # Test bugfix of #6599
    copied_ds = gdal.Translate('', filename, format='MEM')
    data_ds = copied_ds.GetRasterBand(1).ReadAsArray()
    copied_ds = None

    assert numpy.alltrue(data == data_ds)


###############################################################################
# Verify imaginary part extraction from a real dataset.

def test_pixfun_imag_r():

    filename = 'data/pixfun_imag_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == 0)


###############################################################################
# Verify imaginary part extraction from a real dataset.

def test_pixfun_complex():

    filename = 'data/pixfun_complex.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.allclose(data, refdata + 1j * refdata)


###############################################################################
# Verify modulus extraction from a complex (float) dataset.

def test_pixfun_mod_c():

    filename = 'data/pixfun_mod_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    res = numpy.allclose(data, numpy.abs(refdata))
    if gdaltest.is_travis_branch('sanitize') and not res:
        print(data)
        print(numpy.abs(refdata))
        pytest.xfail()

    assert res


###############################################################################
# Verify modulus extraction from a real (integer type) dataset.

def test_pixfun_mod_r():

    filename = 'data/pixfun_mod_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    res = numpy.alltrue(data == numpy.abs(refdata))
    if gdaltest.is_travis_branch('sanitize') and not res:
        print(data)
        print(numpy.abs(refdata))
        pytest.xfail()

    assert res


###############################################################################
# Verify phase extraction from a complex dataset.

def test_pixfun_phase_c():

    filename = 'data/pixfun_phase_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()
    refdata = refdata.astype('complex128')

    assert numpy.allclose(data, numpy.arctan2(refdata.imag, refdata.real))


###############################################################################
# Verify phase extraction from a real dataset.

def test_pixfun_phase_r():

    filename = 'data/pixfun_phase_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/pixfun_imag_c.vrt'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == numpy.arctan2(0, refdata))


###############################################################################
# Verify cmplex conjugare computation on a complex dataset.

def test_pixfun_conj_c():

    filename = 'data/pixfun_conj_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == numpy.conj(refdata))


###############################################################################
# Verify cmplex conjugare computation on a real dataset.

def test_pixfun_conj_r():

    filename = 'data/pixfun_conj_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == numpy.conj(refdata))


###############################################################################
# Verify the sum of 3 (real) datasets.

def test_pixfun_sum_r():

    filename = 'data/pixfun_sum_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    refdata = numpy.zeros(data.shape, 'float')
    for reffilename in ('data/uint16.tif', 'data/int32.tif',
                        'data/float32.tif'):
        refds = gdal.Open(reffilename)
        assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
        refdata += refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == refdata)


###############################################################################
# Verify the sum of 3 (two complex and one real) datasets.

def test_pixfun_sum_c():

    filename = 'data/pixfun_sum_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    refdata = numpy.zeros(data.shape, 'complex')
    for reffilename in ('data/uint16.tif', 'data/cint_sar.tif',
                        'data/cfloat64.tif'):
        refds = gdal.Open(reffilename)
        assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
        refdata += refds.GetRasterBand(1).ReadAsArray(0, 0, 5, 6)

    assert numpy.alltrue(data == refdata)


###############################################################################
# Verify the difference of 2 (real) datasets.

def test_pixfun_diff_r():

    filename = 'data/pixfun_diff_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata1 = refds.GetRasterBand(1).ReadAsArray(0, 0, 5, 6)

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata2 = refds.GetRasterBand(1).ReadAsArray(10, 10, 5, 6)

    assert numpy.alltrue(data == refdata1 - refdata2)


###############################################################################
# Verify the difference of 2 (complex) datasets.

def test_pixfun_diff_c():

    filename = 'data/pixfun_diff_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata1 = refds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cfloat64.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata2 = refds.GetRasterBand(1).ReadAsArray(0, 0, 5, 6)

    assert numpy.alltrue(data == refdata1 - refdata2)


###############################################################################
# Verify the product of 3 (real) datasets.

def test_pixfun_mul_r():

    filename = 'data/pixfun_mul_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    refdata = numpy.ones(data.shape, 'float')
    for reffilename in ('data/uint16.tif', 'data/int32.tif',
                        'data/float32.tif'):
        refds = gdal.Open(reffilename)
        assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
        refdata *= refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == refdata)


###############################################################################
# Verify the product of 2 (complex) datasets.

def test_pixfun_mul_c():

    filename = 'data/pixfun_mul_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == refdata * refdata)


###############################################################################
# Verify the product with complex conjugate of a complex datasets.

def test_pixfun_cmul_c():

    filename = 'data/pixfun_cmul_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == refdata * refdata.conj())


###############################################################################
# Verify the product with complex conjugate of two real datasets.

def test_pixfun_cmul_r():

    filename = 'data/pixfun_cmul_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/uint16.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata1 = refds.GetRasterBand(1).ReadAsArray()
    refdata1 = refdata1.astype('float64')

    reffilename = 'data/int32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata2 = refds.GetRasterBand(1).ReadAsArray()
    refdata2 = refdata2.astype('float64')

    assert numpy.alltrue(data == refdata1 * refdata2.conj())


###############################################################################
# Verify computation of the inverse of a real datasets.

def test_pixfun_inv_r():

    filename = 'data/pixfun_inv_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/uint16.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()
    refdata = refdata.astype('float64')

    assert numpy.alltrue(data == 1. / refdata)


def test_pixfun_inv_r_zero():

    ds = gdal.Open("""<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="Float64" band="1" subClass="VRTDerivedRasterBand">
    <Description>Inverse</Description>
    <PixelFunctionType>inv</PixelFunctionType>
    <SourceTransferType>Float64</SourceTransferType>
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/float32.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <ScaleOffset>0</ScaleOffset>
      <ScaleRatio>0</ScaleRatio>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")
    data = ds.GetRasterBand(1).ReadAsArray()
    assert math.isinf(data[0][0])


###############################################################################
# Verify computation of the inverse of a complex datasets.

def test_pixfun_inv_c():

    filename = 'data/pixfun_inv_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()
    refdata = refdata.astype('complex')
    delta = data - 1. / refdata

    assert numpy.alltrue(abs(delta.real) < 1e-13)
    assert numpy.alltrue(abs(delta.imag) < 1e-13)


def test_pixfun_inv_c_zero():

    ds = gdal.Open("""<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="CFloat64" band="1" subClass="VRTDerivedRasterBand">
    <Description>Inverse</Description>
    <PixelFunctionType>inv</PixelFunctionType>
    <SourceTransferType>CFloat64</SourceTransferType>
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/float32.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <ScaleOffset>0</ScaleOffset>
      <ScaleRatio>0</ScaleRatio>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")
    data = ds.GetRasterBand(1).ReadAsArray()
    assert math.isinf(data[0][0].real)
    assert math.isinf(data[0][0].imag)


###############################################################################
# Verify intensity computation of a complex dataset.

def test_pixfun_intensity_c():

    filename = 'data/pixfun_intensity_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == (refdata * refdata.conj()).real)


###############################################################################
# Verify intensity computation of real dataset.

def test_pixfun_intensity_r():

    filename = 'data/pixfun_intensity_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == (refdata * refdata.conj()).real)


###############################################################################
# Verify square root computation.

def test_pixfun_sqrt():

    filename = 'data/pixfun_sqrt.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == numpy.sqrt(refdata))


###############################################################################
# Verify logarithm computation of real dataset.

def test_pixfun_log10_r():

    filename = 'data/pixfun_log10_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.alltrue(data == numpy.log10(refdata))


###############################################################################
# Verify logarithm computation of imag dataset.

def test_pixfun_log10_c():

    filename = 'data/pixfun_log10_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()
    assert numpy.allclose(data, numpy.log10(numpy.abs(refdata)))


###############################################################################
# Verify dB computation of real dataset.

def test_pixfun_dB_r():

    filename = 'data/pixfun_dB_r.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    assert numpy.allclose(data, 20. * numpy.log10(refdata))


###############################################################################
# Verify dB computation of imag dataset.

def test_pixfun_dB_c():

    filename = 'data/pixfun_dB_c.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/cint_sar.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()
    assert numpy.allclose(data, 20. * numpy.log10(numpy.abs(refdata)))


###############################################################################
# Verify conversion from dB to amplitude.

def test_pixfun_dB2amp():

    filename = 'data/pixfun_dB2amp.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()

    # if not numpy.alltrue(data == 10.**(refdata/20.)):
    assert numpy.allclose(data, 10.**(refdata / 20.))


###############################################################################
# Verify conversion from dB to power.

def test_pixfun_dB2pow():

    filename = 'data/pixfun_dB2pow.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()
    refdata = refdata.astype('float64')

    assert numpy.allclose(data, 10.**(refdata / 10.))


###############################################################################
# Verify raising values to a power

def test_pixfun_pow():

    filename = 'data/pixfun_pow.vrt'
    ds = gdal.OpenShared(filename, gdal.GA_ReadOnly)
    assert ds is not None, ('Unable to open "%s" dataset.' % filename)
    data = ds.GetRasterBand(1).ReadAsArray()

    reffilename = 'data/float32.tif'
    refds = gdal.Open(reffilename)
    assert refds is not None, ('Unable to open "%s" dataset.' % reffilename)
    refdata = refds.GetRasterBand(1).ReadAsArray()
    refdata = refdata.astype('float64')

    assert numpy.allclose(data, refdata**3.14)


###############################################################################
# Verify linear pixel interpolation

def interpolate_vrt(*, fname, bands, method, nx, ny, t0, dt, t):
    vrtXml =  """
    <VRTDataset rasterXSize="{nx}" rasterYSize="{ny}">
      <VRTRasterBand dataType="Float32" band="1" subClass="VRTDerivedRasterBand">
        <Description>Interpolated</Description>
        <PixelFunctionType>interpolate_{method}</PixelFunctionType>
        <PixelFunctionArguments t0="{t0}" dt="{dt}" t="{t}" />
        <SourceTransferType>Float32</SourceTransferType>""".format(nx=nx, ny=ny, t0=t0, dt=dt, t=t, method=method)

    for b in range(1, bands+1):
        vrtXml += """
         <SimpleSource>
          <SourceFilename relativeToVRT="0">{fname}</SourceFilename>
          <SourceBand>{band}</SourceBand>
          <SrcRect xOff="0" yOff="0" xSize="{nx}" ySize="{ny}"/>
          <DstRect xOff="0" yOff="0" xSize="{nx}" ySize="{ny}"/>
        </SimpleSource>
        """.format(fname=fname, band=b, nx=nx, ny=ny)

    vrtXml += """
    </VRTRasterBand>
    </VRTDataset>
    """

    return vrtXml


def test_pixfun_interpolate_linear():

    np = pytest.importorskip('numpy')

    x = np.array([[1, 2], [3, 4]])

    layers = [x, x + 17, x + 23]

    nx = x.shape[0]
    ny = x.shape[1]
    bands = len(layers)

    drv = gdal.GetDriverByName('GTiff')
    fname = "/vsimem/test.tif"
    ds = drv.Create(fname, xsize=nx, ysize=ny, bands=bands, eType=gdal.GDT_Float32)

    for i, lyr in enumerate(layers):
        ds.GetRasterBand(i + 1).WriteArray(lyr)

    ds = None

    # interpolate between bands 2 and 3
    ds = gdal.Open(interpolate_vrt(method='linear', fname=fname, nx=nx, ny=ny, bands=bands, t0=10, dt=5, t=17))
    interpolated = ds.GetRasterBand(1).ReadAsArray()
    assert np.allclose(interpolated, layers[1] + (17-15)*(layers[2]-layers[1])/5)

    ds = gdal.Open(interpolate_vrt(method='exp', fname=fname, nx=nx, ny=ny, bands=bands, t0=10, dt=5, t=17))
    interpolated = ds.GetRasterBand(1).ReadAsArray()
    assert np.allclose(interpolated, layers[1]*np.exp(np.log(layers[2]/layers[1])/5 * (17-15)))

    # extrapolate beyond band 3
    ds = gdal.Open(interpolate_vrt(method='linear', fname=fname, nx=nx, ny=ny, bands=bands, t0=0, dt=10, t=38))
    interpolated = ds.GetRasterBand(1).ReadAsArray()
    assert np.allclose(interpolated, layers[2] + (38-20)*(layers[2]-layers[1])/10)

    ds = gdal.Open(interpolate_vrt(method='linear', fname=fname, nx=nx, ny=ny, bands=bands, t0=0, dt=10, t=28))
    interpolated = ds.GetRasterBand(1).ReadAsArray()
    assert np.allclose(interpolated, layers[2] + (28-20)*(layers[2]-layers[1])/10)

    ds = gdal.Open(interpolate_vrt(method='exp', fname=fname, nx=nx, ny=ny, bands=bands, t0=0, dt=10, t=38))
    interpolated = ds.GetRasterBand(1).ReadAsArray()
    assert np.allclose(interpolated, layers[2]*np.exp(np.log(layers[2]/layers[1])/10 * (38-20)))

    # extrapolate before band 1
    ds = gdal.Open(interpolate_vrt(method='linear', fname=fname, nx=nx, ny=ny, bands=bands, t0=-10, dt=1, t=-22.7))
    interpolated = ds.GetRasterBand(1).ReadAsArray()
    assert np.allclose(interpolated, layers[0] + (-22.7 - -10)*(layers[1]-layers[0])/1)

    ds = gdal.Open(interpolate_vrt(method='exp', fname=fname, nx=nx, ny=ny, bands=bands, t0=-10, dt=1, t=-22.7))
    interpolated = ds.GetRasterBand(1).ReadAsArray()
    assert np.allclose(interpolated, layers[0]*np.exp(np.log(layers[1]/layers[0])/1 * (-22.7 - -10)))

###############################################################################
