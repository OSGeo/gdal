#!/usr/bin/env python
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL Python Interface
#  Purpose:  Application for applying sieve filter to raster data.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2008, Frank Warmerdam
#  Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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


def Usage():
    print("""
gdal_sieve [-q] [-st threshold] [-4] [-8] [-o name=value]
           srcfile [-nomask] [-mask filename] [-of format] [dstfile]
""")
    sys.exit(1)


def DoesDriverHandleExtension(drv, ext):
    exts = drv.GetMetadataItem(gdal.DMD_EXTENSIONS)
    return exts is not None and exts.lower().find(ext.lower()) >= 0


def GetExtension(filename):
    ext = os.path.splitext(filename)[1]
    if ext.startswith('.'):
        ext = ext[1:]
    return ext


def GetOutputDriversFor(filename):
    drv_list = []
    ext = GetExtension(filename)
    for i in range(gdal.GetDriverCount()):
        drv = gdal.GetDriver(i)
        if (drv.GetMetadataItem(gdal.DCAP_CREATE) is not None or
            drv.GetMetadataItem(gdal.DCAP_CREATECOPY) is not None) and \
           drv.GetMetadataItem(gdal.DCAP_RASTER) is not None:
            if ext and DoesDriverHandleExtension(drv, ext):
                drv_list.append(drv.ShortName)
            else:
                prefix = drv.GetMetadataItem(gdal.DMD_CONNECTION_PREFIX)
                if prefix is not None and filename.lower().startswith(prefix.lower()):
                    drv_list.append(drv.ShortName)

    # GMT is registered before netCDF for opening reasons, but we want
    # netCDF to be used by default for output.
    if ext.lower() == 'nc' and not drv_list and \
       drv_list[0].upper() == 'GMT' and drv_list[1].upper() == 'NETCDF':
        drv_list = ['NETCDF', 'GMT']

    return drv_list


def GetOutputDriverFor(filename):
    drv_list = GetOutputDriversFor(filename)
    if not drv_list:
        ext = GetExtension(filename)
        if not ext:
            return 'GTiff'
        else:
            raise Exception("Cannot guess driver for %s" % filename)
    elif len(drv_list) > 1:
        print("Several drivers matching %s extension. Using %s" % (ext, drv_list[0]))
    return drv_list[0]

# =============================================================================
# 	Mainline
# =============================================================================


threshold = 2
connectedness = 4
options = []
quiet_flag = 0
src_filename = None

dst_filename = None
frmt = None

mask = 'default'

gdal.AllRegister()
argv = gdal.GeneralCmdLineProcessor(sys.argv)
if argv is None:
    sys.exit(0)

# Parse command line arguments.
i = 1
while i < len(argv):
    arg = argv[i]

    if arg == '-of' or arg == '-f':
        i = i + 1
        frmt = argv[i]

    elif arg == '-4':
        connectedness = 4

    elif arg == '-8':
        connectedness = 8

    elif arg == '-q' or arg == '-quiet':
        quiet_flag = 1

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
        Usage()

    elif src_filename is None:
        src_filename = argv[i]

    elif dst_filename is None:
        dst_filename = argv[i]

    else:
        Usage()

    i = i + 1

if src_filename is None:
    Usage()

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
    sys.exit(1)

# =============================================================================
# Open source file
# =============================================================================

if dst_filename is None:
    src_ds = gdal.Open(src_filename, gdal.GA_Update)
else:
    src_ds = gdal.Open(src_filename, gdal.GA_ReadOnly)

if src_ds is None:
    print('Unable to open %s ' % src_filename)
    sys.exit(1)

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
    if frmt is None:
        frmt = GetOutputDriverFor(dst_filename)

    drv = gdal.GetDriverByName(frmt)
    dst_ds = drv.Create(dst_filename, src_ds.RasterXSize, src_ds.RasterYSize, 1,
                        srcband.DataType)
    wkt = src_ds.GetProjection()
    if wkt != '':
        dst_ds.SetProjection(wkt)
    dst_ds.SetGeoTransform(src_ds.GetGeoTransform())

    dstband = dst_ds.GetRasterBand(1)
else:
    dstband = srcband

# =============================================================================
# Invoke algorithm.
# =============================================================================

if quiet_flag:
    prog_func = None
else:
    prog_func = gdal.TermProgress_nocb

result = gdal.SieveFilter(srcband, maskband, dstband,
                          threshold, connectedness,
                          callback=prog_func)

src_ds = None
dst_ds = None
mask_ds = None
