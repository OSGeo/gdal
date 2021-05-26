#!/usr/bin/env python3
#
# -*- coding: utf-8 -*-
# ******************************************************************************
#  $Id$
#
#  Name:     wcs_virtds_params.py
#  Project:  GDAL Python Interface
#  Purpose:  Generates MapServer WCS layer definition from a tileindex with mixed SRS
#  Author:   Even Rouault, <even dot rouault at spatialys.com>
#
# ******************************************************************************
#  Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

import os
import sys

from osgeo import gdal
from osgeo import ogr
from osgeo import osr


def Usage():
    print('Usage: wcs_virtds_params.py [-lyr_name name] [-tileindex field_name] [-t_srs srsdef] -src_srs_name field_name ogr_ds_tileindex')
    return 1


def main(argv):
    argv = ogr.GeneralCmdLineProcessor(argv)

    ogr_ds_name = None
    lyr_name = None

    tileitem = 'location'
    tilesrs = None
    srsname = None

    nArgc = len(argv)
    iArg = 1
    while iArg < nArgc:

        if argv[iArg] == "-lyr_name" and iArg < nArgc - 1:
            iArg = iArg + 1
            lyr_name = argv[iArg]

        elif argv[iArg] == "-tileindex" and iArg < nArgc - 1:
            iArg = iArg + 1
            tileitem = argv[iArg]

        elif argv[iArg] == "-src_srs_name" and iArg < nArgc - 1:
            iArg = iArg + 1
            tilesrs = argv[iArg]

        elif argv[iArg] == "-t_srs" and iArg < nArgc - 1:
            iArg = iArg + 1
            srsname = argv[iArg]

        elif argv[iArg][0] == '-':
            return Usage()

        elif ogr_ds_name is None:
            ogr_ds_name = argv[iArg]

        iArg = iArg + 1

    if ogr_ds_name is None or tilesrs is None:
        return Usage()

    ogr_ds = ogr.Open(ogr_ds_name)
    if ogr_ds is None:
        raise Exception('cannot open %s' % ogr_ds_name)
    if ogr_ds.GetLayerCount() == 1:
        lyr = ogr_ds.GetLayer(0)
    elif lyr_name is None:
        raise Exception('-lyr_name should be specified')
    else:
        lyr = ogr_ds.GetLayerByName(lyr_name)

    if lyr.GetLayerDefn().GetFieldIndex(tileitem) < 0:
        raise Exception('%s field cannot be found in layer definition' % tileitem)

    if lyr.GetLayerDefn().GetFieldIndex(tilesrs) < 0:
        raise Exception('%s field cannot be found in layer definition' % tilesrs)

    lyr_srs = lyr.GetSpatialRef()
    if srsname is not None:
        srs = osr.SpatialReference()
        if srs.SetFromUserInput(srsname) != 0:
            raise Exception('invalid value for -t_srs : %s' % srsname)

        # Sanity check
        if lyr_srs is not None:
            lyr_srs_proj4 = lyr_srs.ExportToProj4()
            lyr_srs = osr.SpatialReference()
            lyr_srs.SetFromUserInput(lyr_srs_proj4)
            lyr_srs_proj4 = lyr_srs.ExportToProj4()

            srs_proj4 = srs.ExportToProj4()
            if lyr_srs_proj4 != srs_proj4:
                raise Exception('-t_srs overrides the layer SRS in an incompatible way : (%s, %s)' % (srs_proj4, lyr_srs_proj4))
    else:
        srs = lyr_srs

    if srs is None:
        raise Exception('cannot fetch source SRS')

    srs.AutoIdentifyEPSG()
    authority_name = srs.GetAuthorityName(None)
    authority_code = srs.GetAuthorityCode(None)
    dst_wkt = srs.ExportToWkt()
    if authority_name != 'EPSG' or authority_code is None:
        raise Exception('cannot fetch source SRS as EPSG:XXXX code : %s' % dst_wkt)

    counter = 0
    xres = 0
    yres = 0

    while True:
        feat = lyr.GetNextFeature()
        if feat is None:
            break
        # feat.DumpReadable()

        gdal_ds_name = feat.GetField(tileitem)
        if not os.path.isabs(gdal_ds_name):
            gdal_ds_name = os.path.join(os.path.dirname(ogr_ds_name), gdal_ds_name)
        gdal_ds = gdal.Open(gdal_ds_name)
        if gdal_ds is None:
            raise Exception('cannot open %s' % gdal_ds_name)
        warped_vrt_ds = gdal.AutoCreateWarpedVRT(gdal_ds, None, dst_wkt)
        if warped_vrt_ds is None:
            raise Exception('cannot warp %s to %s' % (gdal_ds_name, dst_wkt))
        gt = warped_vrt_ds.GetGeoTransform()
        xres += gt[1]
        yres += gt[5]
        warped_vrt_ds = None

        counter = counter + 1

    if counter == 0:
        raise Exception('tileindex is empty')

    xres /= counter
    yres /= counter
    (xmin, xmax, ymin, ymax) = lyr.GetExtent()
    xsize = (int)((xmax - xmin) / xres + 0.5)
    ysize = (int)((ymax - ymin) / abs(yres) + 0.5)

    layername = lyr.GetName()

    if ogr_ds.GetDriver().GetName() != 'ESRI Shapefile':
        print("""LAYER
      NAME "%s_tileindex"
      TYPE POLYGON
      STATUS OFF
      CONNECTIONTYPE OGR
      CONNECTION "%s,%s"
      PROJECTION
        "+init=epsg:%s"
      END
    END""" % (layername, ogr_ds_name, lyr.GetName(), authority_code))
        print("")

        tileindex = "%s_tileindex" % layername
    else:
        tileindex = ogr_ds_name

    print("""LAYER
      NAME "%s"
      TYPE RASTER
      STATUS ON
      TILEINDEX "%s"
      TILEITEM "%s"
      TILESRS "%s"
      PROJECTION
        "+init=epsg:%s"
      END
      METADATA
       "wcs_label"       "%s"
       "wcs_rangeset_name"   "Range 1"  ### required to support DescribeCoverage request
       "wcs_rangeset_label"  "My Label" ### required to support DescribeCoverage request
       "wcs_extent"      "%f %f %f %f"
       "wcs_size"        "%d %d"
       "wcs_resolution"  "%f %f"
      END
    END""" % (layername, tileindex, tileitem, tilesrs, authority_code, layername, xmin, ymin, xmax, ymax, xsize, ysize, xres, abs(yres)))

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
