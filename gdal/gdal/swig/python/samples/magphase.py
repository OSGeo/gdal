#!/usr/bin/env python
#******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Example computing the magnitude and phase from a complex image.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
#******************************************************************************
#  Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
#******************************************************************************

import gdal
import gdalnumeric
try:
    import numpy
except:
    import Numeric as numpy


src_ds = gdal.Open('complex.tif')
xsize = src_ds.RasterXSize
ysize = src_ds.RasterYSize

src_image = src_ds.GetRasterBand(1).ReadAsArray()
mag_image = pow(numpy.real(src_image)*numpy.real(src_image) \
                + numpy.imag(src_image)*numpy.imag(src_image),0.5)
gdalnumeric.SaveArray( mag_image, 'magnitude.tif' )

phase_image = numpy.angle(src_image)
gdalnumeric.SaveArray( phase_image, 'phase.tif' )



