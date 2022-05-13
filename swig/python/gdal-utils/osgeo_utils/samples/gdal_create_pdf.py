#!/usr/bin/env python3
###############################################################################
# $Id$
#
#  Project:  GDAL samples
#  Purpose:  Create a PDF from a XML composition file
#  Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2019, Even Rouault<even.rouault at spatialys.com>
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
    print('Usage: gdal_create_pdf composition.xml out.pdf')
    return -1


def gdal_create_pdf(argv):
    srcfile = None
    targetfile = None

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return -1

    for i in range(1, len(argv)):
        if argv[i][0] == '-':
            print('Unrecognized option : %s' % argv[i])
            return Usage()
        elif srcfile is None:
            srcfile = argv[i]
        elif targetfile is None:
            targetfile = argv[i]
        else:
            print('Unexpected option : %s' % argv[i])
            return Usage()

    if srcfile is None or targetfile is None:
        return Usage()

    out_ds = gdal.GetDriverByName("PDF").Create(
        targetfile, 0, 0, 0, gdal.GDT_Unknown,
        options = ['COMPOSITION_FILE=' + srcfile])
    return 0 if out_ds else 1


def main(argv=sys.argv):
    gdal_create_pdf(argv)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
