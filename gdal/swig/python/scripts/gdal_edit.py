#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
#  Project:  GDAL samples
#  Purpose:  Edit in place various information of an existing GDAL dataset
#  Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
#  Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
###############################################################################

import sys

from osgeo import gdal
from osgeo import osr

def Usage():
    print('Usage: gdal_edit [--help-general] [-ro] [-a_srs srs_def] [-a_ullr ulx uly lrx lry]')
    print('                 [-tr xres yres] [-unsetgt] [-a_nodata value] [-unsetnodata]')
    print('                 [-offset value] [-scale value]')
    print('                 [-unsetstats] [-stats] [-approx_stats]')
    print('                 [-gcp pixel line easting northing [elevation]]*')
    print('                 [-unsetmd] [-oo NAME=VALUE]* [-mo "META-TAG=VALUE"]*  datasetname')
    print('')
    print('Edit in place various information of an existing GDAL dataset.')
    return -1


def ArgIsNumeric(s):
    i = 0

    while i < len(s):
        if ( s[i] < '0' or s[i] > '9') and s[i] != '.' and s[i] != 'e' and s[i] != '+' and s[i] != '-':
            return False
        i = i + 1

    return True

def gdal_edit(argv):

    argv = gdal.GeneralCmdLineProcessor( argv )
    if argv is None:
        return -1

    datasetname = None
    srs = None
    ulx = None
    uly = None
    lrx = None
    lry = None
    nodata = None
    unsetnodata = False
    xres = None
    yres = None
    unsetgt = False
    unsetstats = False
    stats = False
    approx_stats = False
    unsetmd = False
    ro = False
    molist = []
    gcp_list = []
    open_options = []
    offset = None
    scale = None

    i = 1
    argc = len(argv)
    while i < argc:
        if argv[i] == '-ro':
            ro = True
        elif argv[i] == '-a_srs' and i < len(argv)-1:
            srs = argv[i+1]
            i = i + 1
        elif argv[i] == '-a_ullr' and i < len(argv)-4:
            ulx = float(argv[i+1])
            i = i + 1
            uly = float(argv[i+1])
            i = i + 1
            lrx = float(argv[i+1])
            i = i + 1
            lry = float(argv[i+1])
            i = i + 1
        elif argv[i] == '-tr' and i < len(argv)-2:
            xres = float(argv[i+1])
            i = i + 1
            yres = float(argv[i+1])
            i = i + 1
        elif argv[i] == '-a_nodata' and i < len(argv)-1:
            nodata = float(argv[i+1])
            i = i + 1
        elif argv[i] == '-scale' and i < len(argv)-1:
            scale = float(argv[i+1])
            i = i + 1
        elif argv[i] == '-offset' and i < len(argv)-1:
            offset = float(argv[i+1])
            i = i + 1
        elif argv[i] == '-mo' and i < len(argv)-1:
            molist.append(argv[i+1])
            i = i + 1
        elif argv[i] == '-gcp' and i + 4 < len(argv):
            pixel = float(argv[i+1])
            i = i + 1
            line = float(argv[i+1])
            i = i + 1
            x = float(argv[i+1])
            i = i + 1
            y = float(argv[i+1])
            i = i + 1
            if i + 1 < len(argv) and ArgIsNumeric(argv[i+1]):
                z = float(argv[i+1])
                i = i + 1
            else:
                z = 0
            gcp = gdal.GCP(x,y,z,pixel,line)
            gcp_list.append(gcp)
        elif argv[i] == '-unsetgt' :
            unsetgt = True
        elif argv[i] == '-unsetstats':
            unsetstats = True
        elif argv[i] == '-approx_stats':
            stats = True
            approx_stats = True
        elif argv[i] == '-stats':
            stats = True
        elif argv[i] == '-unsetmd':
            unsetmd = True
        elif argv[i] == '-unsetnodata':
            unsetnodata = True
        elif argv[i] == '-oo' and i < len(argv)-1:
            open_options.append(argv[i+1])
            i = i + 1
        elif argv[i][0] == '-':
            sys.stderr.write('Unrecognized option : %s\n' % argv[i])
            return Usage()
        elif datasetname is None:
            datasetname = argv[i]
        else:
            sys.stderr.write('Unexpected option : %s\n' % argv[i])
            return Usage()

        i = i + 1

    if datasetname is None:
        return Usage()

    if (srs is None and lry is None and yres is None and not unsetgt
            and not unsetstats and not stats and nodata is None
            and len(molist) == 0 and not unsetmd and len(gcp_list) == 0
            and not unsetnodata
            and scale is None and offset is None):
        print('No option specified')
        print('')
        return Usage()

    exclusive_option = 0
    if lry is not None:
        exclusive_option = exclusive_option + 1
    if yres is not None:
        exclusive_option = exclusive_option + 1
    if unsetgt:
        exclusive_option = exclusive_option + 1
    if exclusive_option > 1:
        print('-a_ullr, -tr and -unsetgt options are exclusive.')
        print('')
        return Usage()

    if unsetstats and stats:
        print('-unsetstats and either -stats or -approx_stats options are exclusive.')
        print('')
        return Usage()

    if unsetnodata and nodata:
        print('-unsetnodata and -nodata options are exclusive.')
        print('')
        return Usage()

    if open_options is not None:
        if ro:
            ds = gdal.OpenEx(datasetname, gdal.OF_RASTER, open_options = open_options)
        else:
            ds = gdal.OpenEx(datasetname, gdal.OF_RASTER | gdal.OF_UPDATE, open_options = open_options)
    # GDAL 1.X compat
    elif ro:
        ds = gdal.Open(datasetname)
    else:
        ds = gdal.Open(datasetname, gdal.GA_Update)
    if ds is None:
        return -1

    wkt = None
    if srs == '' or srs == 'None':
        ds.SetProjection('')
    elif srs is not None:
        sr = osr.SpatialReference()
        if sr.SetFromUserInput(srs) != 0:
            print('Failed to process SRS definition: %s' % srs)
            return -1
        wkt = sr.ExportToWkt()
        if len(gcp_list) == 0:
            ds.SetProjection(wkt)

    if lry is not None:
        gt = [ ulx, (lrx - ulx) / ds.RasterXSize, 0,
               uly, 0, (lry - uly) / ds.RasterYSize ]
        ds.SetGeoTransform(gt)

    if yres is not None:
        gt = ds.GetGeoTransform()
        # Doh ! why is gt a tuple and not an array...
        gt = [ gt[j] for j in range(6) ]
        gt[1] = xres
        gt[5] = yres
        ds.SetGeoTransform(gt)

    if unsetgt:
        ds.SetGeoTransform([0,1,0,0,0,1])

    if len(gcp_list) > 0:
        if wkt is None:
            wkt = ds.GetGCPProjection()
        if wkt is None:
            wkt = ''
        ds.SetGCPs(gcp_list, wkt)

    if nodata is not None:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i+1).SetNoDataValue(nodata)
    elif unsetnodata:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i+1).DeleteNoDataValue()

    if scale is not None:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i+1).SetScale(scale)

    if offset is not None:
       for i in range(ds.RasterCount):
           ds.GetRasterBand(i+1).SetOffset(offset)
 
    if unsetstats:
        for i in range(ds.RasterCount):
            band = ds.GetRasterBand(i+1)
            for key in band.GetMetadata().keys():
                if key.startswith('STATISTICS_'):
                    band.SetMetadataItem(key, None)

    if stats:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(i+1).ComputeStatistics(approx_stats)

    if len(molist) != 0:
        if unsetmd:
            md = {}
        else:
            md = ds.GetMetadata()
        for moitem in molist:
            equal_pos = moitem.find('=')
            if equal_pos > 0:
                md[moitem[0:equal_pos]] = moitem[equal_pos+1:]
        ds.SetMetadata(md)
    elif unsetmd:
        ds.SetMetadata({})

    ds = band = None

    return 0

def main():
    return gdal_edit(sys.argv)

if __name__ == '__main__':
    sys.exit(gdal_edit(sys.argv))
