#!/usr/bin/env python3
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Example computing the magnitude and phase from a complex image.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
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
# ******************************************************************************

import sys

import numpy as np

from osgeo import gdal, gdal_array


def Usage():
    print(
        f"Usage: {sys.argv[0]} -- This is a sample. Read source to know how to use. --"
    )
    return 2


def doit(src_filename, dst_magnitude, dst_phase):
    src_ds = gdal.Open(src_filename)
    xsize = src_ds.RasterXSize
    ysize = src_ds.RasterYSize
    print("{} x {}".format(xsize, ysize))

    src_image = src_ds.GetRasterBand(1).ReadAsArray()
    mag_image = pow(
        np.real(src_image) * np.real(src_image)
        + np.imag(src_image) * np.imag(src_image),
        0.5,
    )
    gdal_array.SaveArray(mag_image, dst_magnitude)

    phase_image = np.angle(src_image)
    gdal_array.SaveArray(phase_image, dst_phase)
    return 0


def main(argv=sys.argv):
    if len(sys.argv) <= 4:
        return Usage()
    # src_filename = 'complex.tif'
    # dst_magnitude = 'magnitude.tif'
    # dst_phase = 'phase.tif'
    if len(argv) > 1:
        src_filename = argv[1]
    if len(argv) > 2:
        dst_magnitude = argv[2]
    if len(argv) > 3:
        dst_phase = argv[3]
    return doit(src_filename, dst_magnitude, dst_phase)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
