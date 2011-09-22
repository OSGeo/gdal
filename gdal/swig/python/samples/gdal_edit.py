#!/usr/bin/env python
###############################################################################
# $Id$
#
#  Project:  GDAL samples
#  Purpose:  Edit in place various information of an existing GDAL dataset
#  Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
#  Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

def Usage():
    print('Usage: gdal_edit [--help-general] [-a_srs srs_def] [-a_ullr ulx uly lrx lry]')
    print('                 [-tr xres yres] [-unsetgt] [-a_nodata value] ')
    print('                 [-mo "META-TAG=VALUE"]*  datasetname')
    print('')
    print('Edit in place various information of an existing GDAL dataset.')
    return -1

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
    xres = None
    yres = None
    unsetgt = False
    molist = []

    i = 1
    argc = len(argv)
    while i < argc:
        if argv[i] == '-a_srs' and i < len(argv)-1:
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
        elif argv[i] == '-mo' and i < len(argv)-1:
            molist.append(argv[i+1])
            i = i + 1
        elif argv[i] == '-unsetgt' :
            unsetgt = True
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

    if srs is None and lry is None and yres is None and not unsetgt and nodata is None and len(molist) == 0:
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

    ds = gdal.Open(datasetname, gdal.GA_Update)
    if ds is None:
        return -1

    if srs is not None:
        ds.SetProjection(srs)

    if lry is not None:
        gt = [ ulx, (lrx - ulx) / ds.RasterXSize, 0,
               uly, 0, (lry - uly) / ds.RasterYSize ]
        ds.SetGeoTransform(gt)

    if yres is not None:
        gt = ds.GetGeoTransform()
        # Doh ! why is gt a tuple and not an array...
        gt = [ gt[i] for i in range(6) ]
        gt[1] = xres
        gt[5] = yres
        ds.SetGeoTransform(gt)

    if unsetgt:
        ds.SetGeoTransform([0,1,0,0,0,1])

    if nodata is not None:
        for i in range(ds.RasterCount):
            ds.GetRasterBand(1).SetNoDataValue(nodata)

    if len(molist) != 0:
        ds.SetMetadata(molist)

    ds = None

    return 0

if __name__ == '__main__':
    sys.exit(gdal_edit(sys.argv))
