#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Generate FITS samples
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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

import fitsio
import os
import numpy as np

data_dir = os.path.join(os.path.dirname(__file__), 'data', 'fits')

fitsio.write(os.path.join(data_dir, 'empty_primary_hdu.fits'), data=None, clobber=True)

filename = os.path.join(data_dir, 'image_in_second_hdu.fits')
with fitsio.FITS(filename,'rw',clobber=True) as fits:
    fits.write(data=None, header={'FOO': 'BAR', 'FOO2': 'BAR2'}, clobber=True)
    fits[-1].write_checksum()
    img = np.arange(2,dtype='B').reshape(2,1)
    fits.write(data=img, header={'FOO':'BAR_override', 'BAR':'BAZ'})
    fits[-1].write_checksum()

filename = os.path.join(data_dir, 'image_in_first_and_second_hdu.fits')
with fitsio.FITS(filename,'rw',clobber=True) as fits:
    img = np.arange(2,dtype='B').reshape(2,1)
    fits.write(data=img, extname='FIRST_IMAGE')
    img = np.arange(3,dtype='B').reshape(3,1)
    fits.write(data=img)

filename = os.path.join(data_dir, 'image_in_second_and_fourth_hdu_table_in_third.fits')
with fitsio.FITS(filename,'rw',clobber=True) as fits:
    fits.write(data=None, header={'FOO': 'BAR'})
    fits[-1].write_checksum()

    img = np.arange(2,dtype='B').reshape(2,1)
    fits.write(data=img, extname='FIRST_IMAGE')
    fits[-1].write_checksum()

    nrows=2
    data = np.zeros(nrows, dtype=[('int','i4'),('double','f8')])
    data['int'] = np.arange(nrows,dtype='i4')
    data['double'] = np.arange(nrows,dtype='f8')
    fits.write_table(data)

    img = np.arange(3,dtype='B').reshape(3,1)
    fits.write(data=img, extname='SECOND_IMAGE')
    fits[-1].write_checksum()


from astropy.io import fits

filename = os.path.join(data_dir, 'binary_table.fits')
if os.path.exists(filename):
    os.unlink(filename)

hdr = fits.Header()
hdr['EXTNAME'] = 'MyTable'
hdu = fits.BinTableHDU.from_columns([
    fits.Column(name='B_scaled_integer', format='B',array=[0, 255, 3]),
    fits.Column(name='B_scaled', format='B',array=[0, 255]),
    fits.Column(name='I_scaled_integer', format='I',array=[-32768, 32767]),
    fits.Column(name='I_scaled', format='I',array=[-32768, 32767]),
    fits.Column(name='J_scaled_integer', format='J',array=[-2147483648, 2147483647]),
    fits.Column(name='J_scaled', format='J',array=[-2147483648, 2147483647]),
    fits.Column(name='K_scaled', format='K',array=[-9223372036854775808, 9223372036854775807]),
    fits.Column(name='E_scaled', format='E',array=[1.25, 2.25]),
    fits.Column(name='D_scaled', format='D',array=[1.25, 2.25]),
    fits.Column(name='C_scaled', format='C',array=[1.25 + 2.25j]),
    fits.Column(name='M_scaled', format='M',array=[1.25 + 2.25j]),

    fits.Column(name='L', format='L',array=[True, False]),
    fits.Column(name='2L', format='2L',array=[[True, False], [False, True]]),
    fits.Column(name='PL', format='PL()',array=[[True, False], [False, True, False], []]),
    fits.Column(name='QL', format='QL()',array=[[True, False], [False, True, False], []]),

    fits.Column(name='X', format='X',array=np.array([[1], [0]], dtype=np.uint8)),
    fits.Column(name='33X', format='33X',array=np.array([[1,1,1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1],
                                                           [1,1,1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1]], dtype=np.uint8)),
    # PX doesn't seem to work

    fits.Column(name='B', format='B',array=[0, 255, 3],null=3),
    fits.Column(name='2B', format='2B',array=[[255,0], [0,255]]),
    fits.Column(name='PB', format='PB()',array=[[255,0], [0,255,0], []]),
    fits.Column(name='BDIM', format='6B',dim='(3,2)',array=[[[0,255,0], [255,0,255]], [[255,255,0], [0,0,255]]]),

    fits.Column(name='I', format='I',array=[-32768, 32767]),
    fits.Column(name='2I', format='2I',array=[[-32768, 32767], [32767, -32768]]),
    fits.Column(name='PI', format='PI()',array=[[-32768, 32767], [32767, 0, -32768], []]),

    fits.Column(name='J', format='J',array=[-2147483648, 2147483647]),
    fits.Column(name='2J', format='2J',array=[[-2147483648, 2147483647], [2147483647, -2147483648]]),
    fits.Column(name='PJ', format='PJ()',array=[[-2147483648, 2147483647], [2147483647, 0, -2147483648], []]),

    fits.Column(name='K', format='K',array=[-9223372036854775808, 9223372036854775807]),
    fits.Column(name='2K', format='2K',array=[[-9223372036854775808, 9223372036854775807], [9223372036854775807, -9223372036854775808]]),
    fits.Column(name='PK', format='PK()',array=[[-9223372036854775808, 9223372036854775807], [9223372036854775807, 0, -9223372036854775808], []]),

    fits.Column(name='A', format='A',array=["A", "B"]),
    fits.Column(name='A2', format='A2',array=["AB", "CD"]),
    fits.Column(name='PA', format='PA()',array=["AB", "CDE"]),
    fits.Column(name='ADIM', format='6A',dim='(2, 3)',array=[["AB", "ab", "Ab"], ["CD", "cd", "Cd"]]),

    fits.Column(name='E', format='E',array=[1.25, 2.25]),
    fits.Column(name='2E', format='2E',array=[[1.25, 2.25], [2.25, 1.25]]),
    fits.Column(name='PE', format='PE()',array=[[1.25, 2.25], [2.25, 1.25, 2.25],[]]),

    fits.Column(name='D', format='D',array=[1.2534, 2.25]),
    fits.Column(name='2D', format='2D',array=[[1.2534, 2.25], [2.2534, 1.25]]),
    fits.Column(name='PD', format='PD()',array=[[1.2534, 2.25], [2.2534, 1.25, 2.25],[]]),

    fits.Column(name='C', format='C',array=[1.25 + 2.25j, 2.25 + 1.25j]),
    fits.Column(name='2C', format='2C',array=[[1.25 + 2.25j, 2.25 + 1.25j],[2.25 + 1.25j, 1.25 + 2.25j]]),
    fits.Column(name='PC', format='PC',array=[[1.25 + 2.25j, 2.25 + 1.25j],[2.25 + 1.25j, 1.25 + 2.25j, 2.25 + 1.25j],[]]),

    fits.Column(name='M', format='M',array=[1.2534 + 2.25j, 2.25 + 1.25j]),
    fits.Column(name='2M', format='2M',array=[[1.2534 + 2.25j, 2.25 + 1.25j],[2.25 + 1.25j, 1.25 + 2.25j]]),
    fits.Column(name='PM', format='PM',array=[[1.2534 + 2.25j, 2.25 + 1.25j],[2.25 + 1.25j, 1.25 + 2.25j, 2.25 + 1.25j],[]]),
], header=hdr)
hdu.writeto(filename)

# Add back zero & scal info with fitsio, since there are some issues with
# astropy for integer data types
with fitsio.FITS(filename,'rw') as f:
    hdu = f[-1]
    hdu.write_key('TZERO1', -128)

    hdu.write_key('TSCAL2', 1.5)
    hdu.write_key('TZERO2', 2.5)

    hdu.write_key('TZERO3', 32768)

    hdu.write_key('TSCAL4', 1.5)
    hdu.write_key('TZERO4', 2.5)

    hdu.write_key('TZERO5', 2147483648)

    hdu.write_key('TSCAL6', 1.5)
    hdu.write_key('TZERO6', 2.5)

    hdu.write_key('TSCAL7', 1.5)
    hdu.write_key('TZERO7', 2.5)

    hdu.write_key('TSCAL8', 1.5)
    hdu.write_key('TZERO8', 2.5)

    hdu.write_key('TSCAL9', 1.5)
    hdu.write_key('TZERO9', 2.5)

    hdu.write_key('TSCAL10', 1.5)
    hdu.write_key('TZERO10', 2.5)

    hdu.write_key('TSCAL11', 1.5)
    hdu.write_key('TZERO11', 2.5)
