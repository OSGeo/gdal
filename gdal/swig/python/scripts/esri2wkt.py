#!/usr/bin/env python3
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL
#  Purpose:  Simple command line program for translating ESRI .prj files
#            into WKT.
#  Author:   Frank Warmerdam, warmerda@home.com
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

import sys

from osgeo import osr

def main(argv):
    if len(argv) < 2:
        print('Usage: esri2wkt.py <esri .prj file>')
        sys.exit(1)

    prj_fd = open(argv[1])
    prj_lines = prj_fd.readlines()
    prj_fd.close()

    for i, prj_line in enumerate(prj_lines):
        prj_lines[i] = prj_line.rstrip()

    prj_srs = osr.SpatialReference()
    err = prj_srs.ImportFromESRI(prj_lines)
    if err != 0:
        print('Error = %d' % err)
    else:
        print(prj_srs.ExportToPrettyWkt())


if __name__ == '__main__':
    sys.exit(main(sys.argv))
