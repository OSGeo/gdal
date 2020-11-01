#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL Python Interface
#  Purpose:  Application for converting raster data to a vector polygon layer.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2008, Frank Warmerdam
#  Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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
from osgeo import ogr
from osgeo.utils.auxiliary.util import GetOutputDriverFor


def Usage():
    print("""gdal_polygonize [-8] [-nomask] [-mask filename] raster_file [-b band|mask]
                [-q] [-f ogr_format] out_file [layer] [fieldname]""")
    return 1


def main(argv):
    frmt = None
    options = []
    quiet_flag = 0
    src_filename = None
    src_band_n = 1

    dst_filename = None
    dst_layername = None
    dst_fieldname = None
    dst_field = -1

    mask = 'default'

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-f' or arg == '-of':
            i = i + 1
            frmt = argv[i]

        elif arg == '-q' or arg == '-quiet':
            quiet_flag = 1

        elif arg == '-8':
            options.append('8CONNECTED=8')

        elif arg == '-nomask':
            mask = 'none'

        elif arg == '-mask':
            i = i + 1
            mask = argv[i]

        elif arg == '-b':
            i = i + 1
            if argv[i].startswith('mask'):
                src_band_n = argv[i]
            else:
                src_band_n = int(argv[i])

        elif src_filename is None:
            src_filename = argv[i]

        elif dst_filename is None:
            dst_filename = argv[i]

        elif dst_layername is None:
            dst_layername = argv[i]

        elif dst_fieldname is None:
            dst_fieldname = argv[i]

        else:
            return Usage()

        i = i + 1

    if src_filename is None or dst_filename is None:
        return Usage()

    if frmt is None:
        frmt = GetOutputDriverFor(dst_filename, is_raster=False)

    if dst_layername is None:
        dst_layername = 'out'

    # =============================================================================
    # 	Verify we have next gen bindings with the polygonize method.
    # =============================================================================
    try:
        gdal.Polygonize
    except AttributeError:
        print('')
        print('gdal.Polygonize() not available.  You are likely using "old gen"')
        print('bindings or an older version of the next gen bindings.')
        print('')
        return 1

    # =============================================================================
    # Open source file
    # =============================================================================

    src_ds = gdal.Open(src_filename)

    if src_ds is None:
        print('Unable to open %s' % src_filename)
        return 1

    if src_band_n == 'mask':
        srcband = src_ds.GetRasterBand(1).GetMaskBand()
        # Workaround the fact that most source bands have no dataset attached
        options.append('DATASET_FOR_GEOREF=' + src_filename)
    elif isinstance(src_band_n, str) and src_band_n.startswith('mask,'):
        srcband = src_ds.GetRasterBand(int(src_band_n[len('mask,'):])).GetMaskBand()
        # Workaround the fact that most source bands have no dataset attached
        options.append('DATASET_FOR_GEOREF=' + src_filename)
    else:
        srcband = src_ds.GetRasterBand(src_band_n)

    if mask == 'default':
        maskband = srcband.GetMaskBand()
    elif mask == 'none':
        maskband = None
    else:
        mask_ds = gdal.Open(mask)
        maskband = mask_ds.GetRasterBand(1)

    # =============================================================================
    #       Try opening the destination file as an existing file.
    # =============================================================================

    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        dst_ds = ogr.Open(dst_filename, update=1)
        gdal.PopErrorHandler()
    except:
        dst_ds = None

    # =============================================================================
    # 	Create output file.
    # =============================================================================
    if dst_ds is None:
        drv = ogr.GetDriverByName(frmt)
        if not quiet_flag:
            print('Creating output %s of format %s.' % (dst_filename, frmt))
        dst_ds = drv.CreateDataSource(dst_filename)

    # =============================================================================
    #       Find or create destination layer.
    # =============================================================================
    try:
        dst_layer = dst_ds.GetLayerByName(dst_layername)
    except:
        dst_layer = None

    if dst_layer is None:

        srs = src_ds.GetSpatialRef()
        dst_layer = dst_ds.CreateLayer(dst_layername, geom_type=ogr.wkbPolygon, srs=srs)

        if dst_fieldname is None:
            dst_fieldname = 'DN'

        fd = ogr.FieldDefn(dst_fieldname, ogr.OFTInteger)
        dst_layer.CreateField(fd)
        dst_field = 0
    else:
        if dst_fieldname is not None:
            dst_field = dst_layer.GetLayerDefn().GetFieldIndex(dst_fieldname)
            if dst_field < 0:
                print("Warning: cannot find field '%s' in layer '%s'" % (dst_fieldname, dst_layername))

    # =============================================================================
    # Invoke algorithm.
    # =============================================================================

    if quiet_flag:
        prog_func = None
    else:
        prog_func = gdal.TermProgress_nocb

    result = gdal.Polygonize(srcband, maskband, dst_layer, dst_field, options,
                             callback=prog_func)

    srcband = None
    src_ds = None
    dst_ds = None
    mask_ds = None

    return result


if __name__ == '__main__':
    sys.exit(main(sys.argv))
