#!/usr/bin/env python3
# ******************************************************************************
#  $Id$
#
#  Name:     gdalimport
#  Project:  GDAL Python Interface
#  Purpose:  Import a GDAL supported file to Tiled GeoTIFF, and build overviews
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2000, Frank Warmerdam
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


def progress_cb(complete, message, cb_data):
    print('%s %d' % (cb_data, complete))


def main(argv=sys.argv):
    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    if len(argv) < 2:
        print("Usage: gdalimport.py [--help-general] source_file [newfile]")
        return 2

    filename = argv[1]
    dataset = gdal.Open(filename)
    if dataset is None:
        print('Unable to open %s' % filename)
        return 1

    geotiff = gdal.GetDriverByName("GTiff")
    if geotiff is None:
        print('GeoTIFF driver not registered.')
        return 1

    if len(argv) < 3:
        newbase, ext = os.path.splitext(os.path.basename(filename))
        newfile = newbase + ".tif"
        i = 0
        while os.path.isfile(newfile):
            i = i + 1
            newfile = newbase + "_" + str(i) + ".tif"
    else:
        newfile = argv[2]

    print('Importing to Tiled GeoTIFF file: %s' % newfile)
    new_dataset = geotiff.CreateCopy(newfile, dataset, 0,
                                     ['TILED=YES', ],
                                     callback=progress_cb,
                                     callback_data='Translate: ')
    dataset = None

    print('Building overviews')
    new_dataset.BuildOverviews("average", callback=progress_cb,
                               callback_data='Overviews: ')
    new_dataset = None

    print('Done')


if __name__ == '__main__':
    sys.exit(main(sys.argv))
