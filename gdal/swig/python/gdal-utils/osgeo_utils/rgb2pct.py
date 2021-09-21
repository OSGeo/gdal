#!/usr/bin/env python3
# ******************************************************************************
#  $Id$
#
#  Name:     rgb2pct
#  Project:  GDAL Python Interface
#  Purpose:  Application for converting an RGB image to a pseudocolored image.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2001, Frank Warmerdam
#  Copyright (c) 2020-2021, Idan Miara <idan@miara.com>
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

import os.path
import sys
import textwrap
from typing import Optional

from osgeo import gdal

from osgeo_utils.auxiliary.base import PathLikeOrStr
from osgeo_utils.auxiliary.gdal_argparse import GDALArgumentParser, GDALScript
from osgeo_utils.auxiliary.util import GetOutputDriverFor, open_ds
from osgeo_utils.auxiliary.color_table import get_color_table


def rgb2pct(src_filename: PathLikeOrStr,
            pct_filename: Optional[PathLikeOrStr] = None, dst_filename: Optional[PathLikeOrStr] = None,
            color_count: int = 256, driver_name: Optional[str] = None):
    # Open source file
    src_ds = open_ds(src_filename)
    if src_ds is None:
        raise Exception(f'Unable to open {src_filename}')

    if src_ds.RasterCount < 3:
        raise Exception(f'{src_filename} has {src_ds.RasterCount} band(s), need 3 for inputs red, green and blue.')

    # Ensure we recognise the driver.
    if not driver_name:
        driver_name = GetOutputDriverFor(dst_filename)

    dst_driver = gdal.GetDriverByName(driver_name)
    if dst_driver is None:
        raise Exception(f'"{driver_name}" driver not registered.')

    # Generate palette
    if pct_filename is None:
        ct = gdal.ColorTable()
        err = gdal.ComputeMedianCutPCT(src_ds.GetRasterBand(1),
                                       src_ds.GetRasterBand(2),
                                       src_ds.GetRasterBand(3),
                                       color_count, ct,
                                       callback=gdal.TermProgress_nocb)
    else:
        ct = get_color_table(pct_filename)

    # Create the working file.  We have to use TIFF since there are few formats
    # that allow setting the color table after creation.

    if driver_name.lower() == 'gtiff':
        tif_filename = dst_filename
    else:
        import tempfile
        tif_filedesc, tif_filename = tempfile.mkstemp(suffix='.tif')

    gtiff_driver = gdal.GetDriverByName('GTiff')

    tif_ds = gtiff_driver.Create(tif_filename,
                                 src_ds.RasterXSize, src_ds.RasterYSize, 1)

    tif_ds.GetRasterBand(1).SetRasterColorTable(ct)

    # ----------------------------------------------------------------------------
    # We should copy projection information and so forth at this point.

    tif_ds.SetProjection(src_ds.GetProjection())
    tif_ds.SetGeoTransform(src_ds.GetGeoTransform())
    if src_ds.GetGCPCount() > 0:
        tif_ds.SetGCPs(src_ds.GetGCPs(), src_ds.GetGCPProjection())

    # ----------------------------------------------------------------------------
    # Actually transfer and dither the data.

    err = gdal.DitherRGB2PCT(src_ds.GetRasterBand(1),
                             src_ds.GetRasterBand(2),
                             src_ds.GetRasterBand(3),
                             tif_ds.GetRasterBand(1),
                             ct,
                             callback=gdal.TermProgress_nocb)
    if err != gdal.CE_None:
        raise Exception('DitherRGB2PCT failed')

    if tif_filename == dst_filename:
        dst_ds = tif_ds
    else:
        dst_ds = dst_driver.CreateCopy(dst_filename or '', tif_ds)
        tif_ds = None
        os.close(tif_filedesc)
        gtiff_driver.Delete(tif_filename)

    return dst_ds


def doit(**kwargs):
    try:
        ds = rgb2pct(**kwargs)
        return ds, 0
    except:
        return None, 1


class RGB2PCT(GDALScript):
    def __init__(self):
        super().__init__()
        self.title = 'Convert a 24bit RGB image to 8bit paletted image'
        self.description = textwrap.dedent('''\
            This utility will compute an optimal pseudo-color table for a given RGB image
            using a median cut algorithm on a downsampled RGB histogram.
            Then it converts the image into a pseudo-colored image using the color table.
            This conversion utilizes Floyd-Steinberg dithering (error diffusion)
            to maximize output image visual quality.''')

    def get_parser(self, argv) -> GDALArgumentParser:
        parser = self.parser

        parser.add_argument("-of", dest="driver_name", metavar="gdal_format",
                            help="Select the output format. "
                                 "if not specified, the format is guessed from the extension. "
                                 "Use the short format name.")

        group = parser.add_mutually_exclusive_group()
        group.add_argument("-n", dest="color_count", type=int, default=256, metavar='color',
                           choices=tuple(range(2, 257)),
                           help="Select the number of colors in the generated color table. "
                                "Defaults to 256. Must be between 2 and 256.")

        group.add_argument("-pct", dest='pct_filename', type=str,
                           help="Extract the color table from <palette_file> instead of computing it. "
                                "can be used to have a consistent color table for multiple files. "
                                "The palette file must be either a raster file in a GDAL supported format with a "
                                "palette or a color file in a supported format (txt, qml, qlr).")

        parser.add_argument("src_filename", type=str, help="The input RGB file.")

        parser.add_argument("dst_filename", type=str, help="The output pseudo-colored file that will be created.")

        return parser

    def doit(self, **kwargs):
        return rgb2pct(**kwargs)


def main(argv):
    return RGB2PCT().main(argv)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
