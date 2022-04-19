#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL
#  Purpose:  Generate the extent of each raster tile in a overview as a vector layer
#  Author:   Even Rouault, <even dot rouault at spatialys dot com>
#
# ******************************************************************************
#  Copyright (c) 2019, Even Rouault, <even dot rouault at spatialys dot com>
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
from osgeo import gdal, ogr
from osgeo_utils.auxiliary.util import GetOutputDriverFor


def Usage():
    print('Usage:  tile_extent_from_raster.py [-f format] [-ovr level] in.tif out.shp')
    return 1


def main(argv=sys.argv):
    i = 1
    output_format = None
    in_filename = None
    out_filename = None
    ovr_level = None
    while i < len(argv):
        if argv[i] == "-f":
            output_format = argv[i + 1]
            i = i + 1
        elif argv[i] == "-ovr":
            ovr_level = int(argv[i + 1])
            i = i + 1
        elif argv[i][0] == '-':
            return Usage()
        elif in_filename is None:
            in_filename = argv[i]
        elif out_filename is None:
            out_filename = argv[i]
        else:
            return Usage()

        i = i + 1

    if out_filename is None:
        return Usage()
    if output_format is None:
        output_format = GetOutputDriverFor(out_filename, is_raster=False)

    src_ds = gdal.Open(in_filename)
    out_ds = gdal.GetDriverByName(output_format).Create(out_filename, 0, 0, 0, gdal.GDT_Unknown)
    first_band = src_ds.GetRasterBand(1)
    main_gt = src_ds.GetGeoTransform()

    for i in ([ovr_level] if ovr_level is not None else range(1+first_band.GetOverviewCount())):
        src_band = first_band if i == 0 else first_band.GetOverview(i-1)
        out_lyr = out_ds.CreateLayer('main_image' if i == 0 else ('overview_%d' % i), geom_type = ogr.wkbPolygon, srs = src_ds.GetSpatialRef())
        blockxsize, blockysize = src_band.GetBlockSize()
        nxblocks = (src_band.XSize + blockxsize - 1) // blockxsize
        nyblocks = (src_band.YSize + blockysize - 1) // blockysize
        gt = [ main_gt[0], main_gt[1] * first_band.XSize / src_band.XSize, 0,
               main_gt[3], 0, main_gt[5] * first_band.YSize / src_band.YSize]
        for y in range(nyblocks):
            ymax = gt[3] + y * blockysize * gt[5]
            ymin = ymax + blockysize * gt[5]
            for x in range(nxblocks):
                xmin = gt[0] + x * blockxsize * gt[1]
                xmax = xmin + blockxsize * gt[1]
                f = ogr.Feature(out_lyr.GetLayerDefn())
                wkt = 'POLYGON((%.18g %.18g,%.18g %.18g,%.18g %.18g,%.18g %.18g,%.18g %.18g))' % (xmin, ymin,
                                                                                                  xmin, ymax,
                                                                                                  xmax, ymax,
                                                                                                  xmax, ymin,
                                                                                                  xmin, ymin)
                f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
                out_lyr.CreateFeature(f)
    out_ds = None
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
