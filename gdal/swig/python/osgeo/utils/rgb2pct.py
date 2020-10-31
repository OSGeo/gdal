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

from osgeo import gdal
from osgeo.auxiliary.base import GetOutputDriverFor



def Usage():
    print('Usage: rgb2pct.py [-n colors | -pct palette_file] [-of format] source_file dest_file')
    return 1


def main(argv):
    color_count = 256
    frmt = None
    src_filename = None
    dst_filename = None
    pct_filename = None

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-of' or arg == '-f':
            i = i + 1
            frmt = argv[i]

        elif arg == '-n':
            i = i + 1
            color_count = int(argv[i])

        elif arg == '-pct':
            i = i + 1
            pct_filename = argv[i]

        elif src_filename is None:
            src_filename = argv[i]

        elif dst_filename is None:
            dst_filename = argv[i]

        else:
            return Usage()

        i = i + 1

    if dst_filename is None:
        return Usage()

    _ds, err = doit(pct_filename, src_filename, dst_filename, color_count, frmt)
    return err


def doit(pct_filename=None, src_filename=None, dst_filename=None, color_count=256, frmt=None):
    # Open source file
    src_ds = gdal.Open(src_filename)
    if src_ds is None:
        print('Unable to open %s' % src_filename)
        return None, 1

    if src_ds.RasterCount < 3:
        print('%s has %d band(s), need 3 for inputs red, green and blue.'
              % (src_filename, src_ds.RasterCount))
        return None, 1

    # Ensure we recognise the driver.

    if frmt is None:
        frmt = GetOutputDriverFor(dst_filename)

    dst_driver = gdal.GetDriverByName(frmt)
    if dst_driver is None:
        print('"%s" driver not registered.' % frmt)
        return None, 1

    # Generate palette

    ct = gdal.ColorTable()
    if pct_filename is None:
        err = gdal.ComputeMedianCutPCT(src_ds.GetRasterBand(1),
                                       src_ds.GetRasterBand(2),
                                       src_ds.GetRasterBand(3),
                                       color_count, ct,
                                       callback=gdal.TermProgress_nocb)
    else:
        pct_ds = gdal.Open(pct_filename)
        ct = pct_ds.GetRasterBand(1).GetRasterColorTable().Clone()

    # Create the working file.  We have to use TIFF since there are few formats
    # that allow setting the color table after creation.

    if format == 'GTiff':
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

    if tif_filename == dst_filename:
        dst_ds = tif_ds
    else:
        dst_ds = dst_driver.CreateCopy(dst_filename, tif_ds)
        tif_ds = None
        gtiff_driver.Delete(tif_filename)
        os.close(tif_filedesc)

    return dst_ds, err


if __name__ == '__main__':
    sys.exit(main(sys.argv))
