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
