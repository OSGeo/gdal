#!/usr/bin/env python3
# ******************************************************************************
#  $Id$
#
#  Name:     pct2rgb
#  Project:  GDAL Python Interface
#  Purpose:  Utility to convert paletted images into RGB (or RGBA) images.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2001, Frank Warmerdam
#  Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
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

import sys
import textwrap
from typing import Optional

import numpy as np

from osgeo import gdal

from osgeo_utils.auxiliary.base import PathLikeOrStr
from osgeo_utils.auxiliary.gdal_argparse import GDALArgumentParser, GDALScript
from osgeo_utils.auxiliary.util import GetOutputDriverFor, open_ds
from osgeo_utils.auxiliary.color_palette import get_color_palette
from osgeo_utils.auxiliary.color_table import get_color_table

progress = gdal.TermProgress_nocb


def pct2rgb(src_filename: PathLikeOrStr, pct_filename: Optional[PathLikeOrStr], dst_filename: PathLikeOrStr,
            band_number: int = 1, out_bands: int = 3, driver_name: Optional[str] = None):
    # Open source file
    src_ds = open_ds(src_filename)
    if src_ds is None:
        raise Exception(f'Unable to open {src_filename} ')

    src_band = src_ds.GetRasterBand(band_number)

    # ----------------------------------------------------------------------------
    # Ensure we recognise the driver.

    if driver_name is None:
        driver_name = GetOutputDriverFor(dst_filename)

    dst_driver = gdal.GetDriverByName(driver_name)
    if dst_driver is None:
        raise Exception(f'"{driver_name}" driver not registered.')

    # ----------------------------------------------------------------------------
    # Build color table.

    if pct_filename is not None:
        pal = get_color_palette(pct_filename)
        if pal.has_percents():
            min_val = src_band.GetMinimum()
            max_val = src_band.GetMinimum()
            pal.apply_percent(min_val, max_val)
        ct = get_color_table(pal)
    else:
        ct = src_band.GetRasterColorTable()

    ct_size = ct.GetCount()
    lookup = [np.arange(ct_size),
              np.arange(ct_size),
              np.arange(ct_size),
              np.ones(ct_size) * 255]

    if ct is not None:
        for i in range(ct_size):
            entry = ct.GetColorEntry(i)
            for c in range(4):
                lookup[c][i] = entry[c]

    # ----------------------------------------------------------------------------
    # Create the working file.

    if driver_name.lower() == 'gtiff':
        tif_filename = dst_filename
    else:
        tif_filename = 'temp.tif'

    gtiff_driver = gdal.GetDriverByName('GTiff')

    tif_ds = gtiff_driver.Create(tif_filename, src_ds.RasterXSize, src_ds.RasterYSize, out_bands)

    # ----------------------------------------------------------------------------
    # We should copy projection information and so forth at this point.

    tif_ds.SetProjection(src_ds.GetProjection())
    tif_ds.SetGeoTransform(src_ds.GetGeoTransform())
    if src_ds.GetGCPCount() > 0:
        tif_ds.SetGCPs(src_ds.GetGCPs(), src_ds.GetGCPProjection())

    # ----------------------------------------------------------------------------
    # Do the processing one scanline at a time.

    progress(0.0)
    for iY in range(src_ds.RasterYSize):
        src_data = src_band.ReadAsArray(0, iY, src_ds.RasterXSize, 1)

        for iBand in range(out_bands):
            band_lookup = lookup[iBand]

            dst_data = np.take(band_lookup, src_data)
            tif_ds.GetRasterBand(iBand + 1).WriteArray(dst_data, 0, iY)

        progress((iY + 1.0) / src_ds.RasterYSize)

    # ----------------------------------------------------------------------------
    # Translate intermediate file to output format if desired format is not TIFF.

    if tif_filename == dst_filename:
        dst_ds = tif_ds
    else:
        dst_ds = dst_driver.CreateCopy(dst_filename or '', tif_ds)
        tif_ds = None
        gtiff_driver.Delete(tif_filename)

    return dst_ds


def doit(**kwargs):
    try:
        ds = pct2rgb(**kwargs)
        return ds, 0
    except:
        return None, 1


class PCT2RGB(GDALScript):
    def __init__(self):
        super().__init__()
        self.title = 'Convert an 8bit paletted image to 24bit RGB'
        self.description = textwrap.dedent('''\
            This utility will convert a pseudo-color band on the input file
            into an output RGB file of the desired format.''')

    def get_parser(self, argv) -> GDALArgumentParser:
        parser = self.parser

        parser.add_argument("-of", dest="driver_name", metavar="gdal_format",
                            help="Select the output format. if not specified, the format is guessed from the extension. "
                                 "Use the short format name. "
                                 "Only output formats supporting pseudo-color tables should be used.")

        parser.add_argument("-rgba", dest="out_bands", action="store_const", const=4, default=3,
                            help="Generate a RGBA file (instead of a RGB file by default).")

        parser.add_argument("-b", "-band", dest="band_number", metavar="band", type=int, default=1,
                            help="Band to convert to RGB, defaults to 1.")

        parser.add_argument("-pct", dest='pct_filename', type=str,
                            help="Extract the color table from <palette_file> instead of computing it. "
                                 "can be used to have a consistent color table for multiple files. "
                                 "The palette file must be either a raster file in a GDAL supported format with a "
                                 "palette or a color file in a supported format (txt, qml, qlr).")

        parser.add_argument("src_filename", type=str, help="The input file.")

        parser.add_argument("dst_filename", type=str, help="The output RGB file that will be created.")

        return parser

    def doit(self, **kwargs):
        return pct2rgb(**kwargs)


def main(argv):
    return PCT2RGB().main(argv)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
