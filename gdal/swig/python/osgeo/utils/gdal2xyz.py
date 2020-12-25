#!/usr/bin/env python3
###############################################################################
# $Id$
#
# Project:  GDAL
# Purpose:  Script to translate GDAL supported raster into XYZ ASCII
#           point stream.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2020, Idan Miara <idan@miara.com>
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
from numbers import Number
from pathlib import Path
from typing import Optional, Union, Sequence

from osgeo import gdal

try:
    import numpy as Numeric
except ImportError:
    import Numeric


# =============================================================================


def Usage():
    print('Usage: gdal2xyz.py [-skip factor] [-srcwin xoff yoff width height]')
    print('                   [-band b] [--allBands] [-csv]')
    print('                   [--skipNoData] [-noDataValue value]')
    print('                   srcfile [dstfile]')
    print('')
    return 1


def main(argv):
    srcwin = None
    skip = 1
    srcfile = None
    dstfile = None
    band_nums = []
    all_bands = False
    delim = ' '
    skip_no_data = False
    no_data_value = []

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-srcwin':
            srcwin = (int(argv[i + 1]), int(argv[i + 2]),
                      int(argv[i + 3]), int(argv[i + 4]))
            i = i + 4

        elif arg == '-skip':
            skip = int(argv[i + 1])
            i = i + 1

        elif arg == '-band':
            band_nums.append(int(argv[i + 1]))
            i = i + 1

        elif arg == '--allBands':
            all_bands = True

        elif arg == '-csv':
            delim = ','

        elif arg == '--skipNoData':
            skip_no_data = True

        elif arg == '-noDataValue':
            no_data_value.append(num(argv[i + 1]))
            i = i + 1

        elif arg[0] == '-':
            return Usage()

        elif srcfile is None:
            srcfile = arg

        elif dstfile is None:
            dstfile = arg

        else:
            return Usage()

        i = i + 1

    if srcfile is None:
        return Usage()

    if all_bands:
        band_nums = None
    elif not band_nums:
        band_nums = 1
    return gdal2xyz(srcfile=srcfile, dstfile=dstfile, srcwin=srcwin, skip=skip,
                    band_nums=band_nums, delim=delim,
                    skip_no_data=skip_no_data, no_data_value=no_data_value)


def gdal2xyz(srcfile: Union[str, Path, gdal.Dataset], dstfile: Union[str, Path] = None,
             srcwin: Optional[Sequence[int]] = None,
             skip: Union[int, Sequence[int]] = 1,
             band_nums: Optional[Sequence[int]] = None, delim=' ',
             skip_no_data: bool = False, no_data_value: Optional[Union[Sequence, Number]] = None):
    """
    translates a raster file (or dataset) into xyz format

    srcfile - source filename or dataset
    dstfile - destination filename
    srcwin - a sequence of 4 integers [x_off y_off x_size y_size]
    skip - how many rows/cols to skip each iteration
    band_nums - number of the bands to include in the output. None to include all bands.
    delim - delimiter to use to separate columns
    skip_no_data - `True` will skip lines with uninteresting data values (NoDataValue or a given value)
    no_data_value - determinates which values to skip
        `None` - skip lines which have the dataset NoDataValue;
        Sequence/Number - skip lines with a given value (per band or per dataset)
    """

    # Open source file.
    srcds = gdal.Open(str(srcfile), gdal.GA_ReadOnly) if isinstance(srcfile, (Path, str)) else srcfile
    if srcds is None:
        print('Could not open %s.' % srcfile)
        return 1

    if not band_nums:
        band_nums = list(range(1, srcds.RasterCount + 1))
    elif isinstance(band_nums, int):
        band_nums = [band_nums]
    bands = []
    for band_num in band_nums:
        band = srcds.GetRasterBand(band_num)
        if band is None:
            print('Could not get band %d' % band_num)
            return 1
        bands.append(band)

    gt = srcds.GetGeoTransform()

    # Collect information on all the source files.
    if srcwin is None:
        srcwin = (0, 0, srcds.RasterXSize, srcds.RasterYSize)

    # Open the output file.
    if dstfile is not None:
        dst_fh = open(dstfile, 'wt')
    else:
        dst_fh = sys.stdout

    dt = srcds.GetRasterBand(1).DataType
    if dt == gdal.GDT_Int32 or dt == gdal.GDT_UInt32:
        band_format = (("%d" + delim) * len(bands)).rstrip(delim) + '\n'
    else:
        band_format = (("%g" + delim) * len(bands)).rstrip(delim) + '\n'

    # Setup an appropriate print format.
    if abs(gt[0]) < 180 and abs(gt[3]) < 180 \
        and abs(srcds.RasterXSize * gt[1]) < 180 \
        and abs(srcds.RasterYSize * gt[5]) < 180:
        frmt = '%.10g' + delim + '%.10g' + delim + '%s'
    else:
        frmt = '%.3f' + delim + '%.3f' + delim + '%s'

    if skip_no_data:
        no_data_value = \
            [no_data_value] if isinstance(no_data_value, Number) else \
            list(band.GetNoDataValue() for band in bands) if not no_data_value else \
            list(no_data_value)

    if isinstance(skip, Sequence):
        x_skip, y_skip = skip
    else:
        x_skip = y_skip = skip

    x_off, y_off, x_size, y_size = srcwin

    # Loop emitting data.

    for y in range(y_off, y_off + y_size, y_skip):

        data = []
        for band in bands:
            band_data = band.ReadAsArray(x_off, y, x_size, 1)
            band_data = Numeric.reshape(band_data, (x_size,))

            data.append(band_data)

        for x_i in range(0, x_size, x_skip):

            x_i_data = []
            for i in range(len(bands)):
                x_i_data.append(data[i][x_i])
            if no_data_value == x_i_data:
                continue

            x = x_i + x_off

            geo_x = gt[0] + (x + 0.5) * gt[1] + (y + 0.5) * gt[2]
            geo_y = gt[3] + (x + 0.5) * gt[4] + (y + 0.5) * gt[5]

            band_str = band_format % tuple(x_i_data)

            line = frmt % (float(geo_x), float(geo_y), band_str)

            dst_fh.write(line)


def num(s: str) -> Number:
    try:
        return int(s)
    except ValueError:
        return float(s)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
