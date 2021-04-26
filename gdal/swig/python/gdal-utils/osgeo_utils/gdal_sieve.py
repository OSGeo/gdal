#!/usr/bin/env python3
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL Python Interface
#  Purpose:  Application for applying sieve filter to raster data.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2008, Frank Warmerdam
#  Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
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
from typing import Optional

from osgeo import gdal

from osgeo_utils.auxiliary.base import PathLikeOrStr
from osgeo_utils.auxiliary.util import GetOutputDriverFor


def Usage():
    print("""gdal_sieve [-q] [-st threshold] [-4] [-8] [-o name=value]
           srcfile [-nomask] [-mask filename] [-of format] [dstfile]""")
    return 1


def main(argv):
    threshold = 2
    connectedness = 4
    quiet = False
    src_filename = None

    dst_filename = None
    driver_name = None

    mask = 'default'

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-of' or arg == '-f':
            i = i + 1
            driver_name = argv[i]

        elif arg == '-4':
            connectedness = 4

        elif arg == '-8':
            connectedness = 8

        elif arg == '-q' or arg == '-quiet':
            quiet = True

        elif arg == '-st':
            i = i + 1
            threshold = int(argv[i])

        elif arg == '-nomask':
            mask = 'none'

        elif arg == '-mask':
            i = i + 1
            mask = argv[i]

        elif arg == '-mask':
            i = i + 1
            mask = argv[i]

        elif arg[:2] == '-h':
            return Usage()

        elif src_filename is None:
            src_filename = argv[i]

        elif dst_filename is None:
            dst_filename = argv[i]

        else:
            return Usage()

        i = i + 1

    if src_filename is None:
        return Usage()

    return gdal_sieve(src_filename=src_filename, dst_filename=dst_filename, driver_name=driver_name,
                      mask=mask, threshold=threshold, connectedness=connectedness, quiet=quiet)


def gdal_sieve(src_filename: Optional[str] = None,
               dst_filename: PathLikeOrStr = None, driver_name: Optional[str] = None, mask: str = 'default',
               threshold: int = 2, connectedness: int = 4,
               quiet: bool = False):
    # =============================================================================
    # 	Verify we have next gen bindings with the sievefilter method.
    # =============================================================================
    try:
        gdal.SieveFilter
    except AttributeError:
        print('')
        print('gdal.SieveFilter() not available.  You are likely using "old gen"')
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
        print('Unable to open %s ' % src_filename)
        return 1

    srcband = src_ds.GetRasterBand(1)

    if mask == 'default':
        maskband = srcband.GetMaskBand()
    elif mask == 'none':
        maskband = None
    else:
        mask_ds = gdal.Open(mask)
        maskband = mask_ds.GetRasterBand(1)

    # =============================================================================
    #       Create output file if one is specified.
    # =============================================================================

    if dst_filename is not None:
        if driver_name is None:
            driver_name = GetOutputDriverFor(dst_filename)

        drv = gdal.GetDriverByName(driver_name)
        dst_ds = drv.Create(dst_filename, src_ds.RasterXSize, src_ds.RasterYSize, 1,
                            srcband.DataType)
        wkt = src_ds.GetProjection()
        if wkt != '':
            dst_ds.SetProjection(wkt)
        gt = src_ds.GetGeoTransform(can_return_null=True)
        if gt is not None:
            dst_ds.SetGeoTransform(gt)

        dstband = dst_ds.GetRasterBand(1)
    else:
        dstband = srcband

    # =============================================================================
    # Invoke algorithm.
    # =============================================================================

    if quiet:
        prog_func = None
    else:
        prog_func = gdal.TermProgress_nocb

    result = gdal.SieveFilter(srcband, maskband, dstband,
                              threshold, connectedness,
                              callback=prog_func)

    src_ds = None
    dst_ds = None
    mask_ds = None

    return result

if __name__ == '__main__':
    sys.exit(main(sys.argv))
