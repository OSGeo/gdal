#!/usr/bin/env python3
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL Python Interface
#  Purpose:  Application for filling nodata areas in a raster by interpolation
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2008, Frank Warmerdam
#  Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
#  Copyright (c) 2021, Idan Miara <idan@miara.com>
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
import textwrap
from numbers import Real
from typing import Optional

from osgeo import gdal

from osgeo_utils.auxiliary.gdal_argparse import GDALArgumentParser, GDALScript


def CopyBand(srcband, dstband):
    for line in range(srcband.YSize):
        line_data = srcband.ReadRaster(0, line, srcband.XSize, 1)
        dstband.WriteRaster(0, line, srcband.XSize, 1, line_data,
                            buf_type=srcband.DataType)


def gdal_fillnodata(src_filename: Optional[str] = None, band_number: int = 1,
                    dst_filename: Optional[str] = None, driver_name: str = 'GTiff',
                    creation_options: Optional[list] = None, quiet: bool = False, mask: str = 'default',
                    max_distance: Real = 100, smoothing_iterations: int = 0, options: Optional[list] = None):
    options = options or []
    creation_options = creation_options or []

    # =============================================================================
    # 	Verify we have next gen bindings with the sievefilter method.
    # =============================================================================
    try:
        gdal.FillNodata
    except AttributeError:
        print('')
        print('gdal.FillNodata() not available.  You are likely using "old gen"')
        print('bindings or an older version of the next gen bindings.')
        print('')
        return 1

    # =============================================================================
    # Open source file
    # =============================================================================

    if dst_filename is None:
        src_ds = gdal.Open(src_filename, gdal.GA_Update)
    else:
        src_ds = gdal.Open(src_filename, gdal.GA_ReadOnly)

    if src_ds is None:
        print('Unable to open %s' % src_filename)
        return 1

    srcband = src_ds.GetRasterBand(band_number)

    # =============================================================================
    #       Create output file if one is specified.
    # =============================================================================

    if dst_filename is not None:

        drv = gdal.GetDriverByName(driver_name)
        dst_ds = drv.Create(dst_filename, src_ds.RasterXSize, src_ds.RasterYSize, 1,
                            srcband.DataType, creation_options)
        wkt = src_ds.GetProjection()
        if wkt != '':
            dst_ds.SetProjection(wkt)
        gt = src_ds.GetGeoTransform(can_return_null=True)
        if gt:
            dst_ds.SetGeoTransform(gt)

        dstband = dst_ds.GetRasterBand(1)
        ndv = srcband.GetNoDataValue()
        if ndv is not None:
            dstband.SetNoDataValue(ndv)

        color_interp = srcband.GetColorInterpretation()
        dstband.SetColorInterpretation(color_interp)
        if color_interp == gdal.GCI_PaletteIndex:
            color_table = srcband.GetColorTable()
            dstband.SetColorTable(color_table)

        CopyBand(srcband, dstband)

    else:
        dstband = srcband

    # =============================================================================
    # Invoke algorithm.
    # =============================================================================

    if quiet:
        prog_func = None
    else:
        prog_func = gdal.TermProgress_nocb

    if mask == 'default':
        maskband = dstband.GetMaskBand()
    else:
        mask_ds = gdal.Open(mask)
        maskband = mask_ds.GetRasterBand(1)

    result = gdal.FillNodata(dstband, maskband,
                             max_distance, smoothing_iterations, options,
                             callback=prog_func)

    src_ds = None
    dst_ds = None
    mask_ds = None

    return result


class GDALFillNoData(GDALScript):
    def __init__(self):
        super().__init__()
        self.title = 'Fill raster regions by interpolation from edges'
        self.description = textwrap.dedent('''\
            It Fills selection regions (usually nodata areas)
            by interpolating from valid pixels around the edges of the area.
            Additional details on the algorithm are available in the GDALFillNodata() docs.''')

    def get_parser(self, argv) -> GDALArgumentParser:
        parser = self.parser

        parser.add_argument('-q', "-quiet", dest="quiet", action="store_true",
                            help="The script runs in quiet mode. "
                                 "The progress monitor is suppressed and routine messages are not displayed.")

        parser.add_argument("-md", dest="max_distance", type=float, default=100, metavar='max_distance',
                            help="The maximum distance (in pixels) "
                                 "that the algorithm will search out for values to interpolate. "
                                 "The default is 100 pixels.")

        parser.add_argument("-si", dest="smoothing_iterations", type=int, default=0, metavar='smoothing_iterations',
                            help="The number of 3x3 average filter smoothing iterations to run after the interpolation "
                                 "to dampen artifacts. The default is zero smoothing iterations.")

        parser.add_argument("-o", dest='options', type=str, action="extend", nargs='*', metavar='name=value',
                            help="Specify a special argument to the algorithm.")

        parser.add_argument("-mask", dest="mask", type=str, metavar='filename', default='default',
                            help="Use the first band of the specified file as a validity mask "
                                 "(zero is invalid, non-zero is valid).")

        parser.add_argument("-b", "-band", dest="band_number", metavar="band", type=int, default=1,
                            help="The band to operate on, defaults to 1.")

        parser.add_argument("-of", dest="driver_name", default='GTiff', metavar="gdal_format",
                            help="Select the output format. Use the short format name.")

        parser.add_argument("-co", dest='creation_options', type=str, action="extend", nargs='*', metavar='name=value',
                            help="Creation options for the destination dataset.")

        parser.add_argument("src_filename", type=str, help="The source raster file used to identify target pixels. "
                                                           "Only one band is used.")

        parser.add_argument("dst_filename", type=str, help="The new file to create with the interpolated result. "
                                                           "If not provided, the source band is updated in place.")

        return parser

    def doit(self, **kwargs):
        return gdal_fillnodata(**kwargs)


def main(argv=sys.argv):
    return GDALFillNoData().main(argv)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
