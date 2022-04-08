#!/usr/bin/env python3
# ******************************************************************************
#  $Id$
#
#  Name:     gcps2wld
#  Project:  GDAL Python Interface
#  Purpose:  Translate the set of GCPs on a file into first order approximation
#            in world file format.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2002, Frank Warmerdam
#  Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
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


def main(argv=sys.argv):
    if len(argv) < 2:
        print("Usage: gcps2wld.py source_file")
        return 2

    filename = argv[1]
    dataset = gdal.Open(filename)
    if dataset is None:
        print('Unable to open %s' % filename)
        return 1

    gcps = dataset.GetGCPs()

    if gcps is None or not gcps:
        print('No GCPs found on file ' + filename)
        return 1

    geotransform = gdal.GCPsToGeoTransform(gcps)

    if geotransform is None:
        print('Unable to extract a geotransform.')
        return 1

    print(geotransform[1])
    print(geotransform[4])
    print(geotransform[2])
    print(geotransform[5])
    print(geotransform[0] + 0.5 * geotransform[1] + 0.5 * geotransform[2])
    print(geotransform[3] + 0.5 * geotransform[4] + 0.5 * geotransform[5])


if __name__ == '__main__':
    sys.exit(main(sys.argv))
