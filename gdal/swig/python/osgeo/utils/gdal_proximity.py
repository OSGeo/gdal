#!/usr/bin/env python3
# ******************************************************************************
#  $Id$
#
#  Name:     gdalproximity
#  Project:  GDAL Python Interface
#  Purpose:  Application for computing raster proximity maps.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2008, Frank Warmerdam
#  Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal
from osgeo.utils.auxiliary.util import GetOutputDriverFor


def Usage():
    print("""
gdal_proximity.py srcfile dstfile [-srcband n] [-dstband n]
                  [-of format] [-co name=value]*
                  [-ot Byte/UInt16/UInt32/Float32/etc]
                  [-values n,n,n] [-distunits PIXEL/GEO]
                  [-maxdist n] [-nodata n] [-use_input_nodata YES/NO]
                  [-fixed-buf-val n] [-q] """)
    return 1


def main(argv):
    frmt = None
    creation_options = []
    options = []
    src_filename = None
    src_band_n = 1
    dst_filename = None
    dst_band_n = 1
    creation_type = 'Float32'
    quiet_flag = 0

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

        elif arg == '-co':
            i = i + 1
            creation_options.append(argv[i])

        elif arg == '-ot':
            i = i + 1
            creation_type = argv[i]

        elif arg == '-maxdist':
            i = i + 1
            options.append('MAXDIST=' + argv[i])

        elif arg == '-values':
            i = i + 1
            options.append('VALUES=' + argv[i])

        elif arg == '-distunits':
            i = i + 1
            options.append('DISTUNITS=' + argv[i])

        elif arg == '-nodata':
            i = i + 1
            options.append('NODATA=' + argv[i])

        elif arg == '-use_input_nodata':
            i = i + 1
            options.append('USE_INPUT_NODATA=' + argv[i])

        elif arg == '-fixed-buf-val':
            i = i + 1
            options.append('FIXED_BUF_VAL=' + argv[i])

        elif arg == '-srcband':
            i = i + 1
            src_band_n = int(argv[i])

        elif arg == '-dstband':
            i = i + 1
            dst_band_n = int(argv[i])

        elif arg == '-q' or arg == '-quiet':
            quiet_flag = 1

        elif src_filename is None:
            src_filename = argv[i]

        elif dst_filename is None:
            dst_filename = argv[i]

        else:
            return Usage()

        i = i + 1

    if src_filename is None or dst_filename is None:
        return Usage()

    # =============================================================================
    #    Open source file
    # =============================================================================

    src_ds = gdal.Open(src_filename)

    if src_ds is None:
        print('Unable to open %s' % src_filename)
        return 1

    srcband = src_ds.GetRasterBand(src_band_n)

    # =============================================================================
    #       Try opening the destination file as an existing file.
    # =============================================================================

    try:
        driver = gdal.IdentifyDriver(dst_filename)
        if driver is not None:
            dst_ds = gdal.Open(dst_filename, gdal.GA_Update)
            dstband = dst_ds.GetRasterBand(dst_band_n)
        else:
            dst_ds = None
    except:
        dst_ds = None

    # =============================================================================
    #     Create output file.
    # =============================================================================
    if dst_ds is None:
        if frmt is None:
            frmt = GetOutputDriverFor(dst_filename)

        drv = gdal.GetDriverByName(frmt)
        dst_ds = drv.Create(dst_filename,
                            src_ds.RasterXSize, src_ds.RasterYSize, 1,
                            gdal.GetDataTypeByName(creation_type), creation_options)

        dst_ds.SetGeoTransform(src_ds.GetGeoTransform())
        dst_ds.SetProjection(src_ds.GetProjectionRef())

        dstband = dst_ds.GetRasterBand(1)

    # =============================================================================
    #    Invoke algorithm.
    # =============================================================================

    if quiet_flag:
        prog_func = None
    else:
        prog_func = gdal.TermProgress_nocb

    gdal.ComputeProximity(srcband, dstband, options,
                          callback=prog_func)

    srcband = None
    dstband = None
    src_ds = None
    dst_ds = None


if __name__ == '__main__':
    sys.exit(main(sys.argv))
