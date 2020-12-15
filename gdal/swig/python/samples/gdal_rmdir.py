#!/usr/bin/env python3
###############################################################################
# $Id$
#
#  Project:  GDAL samples
#  Purpose:  Delete a virtual directory
#  Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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
    print('Usage: gdal_rmdir filename')
    return -1


def gdal_rm(argv, progress=None):
    # pylint: disable=unused-argument
    filename = None
    recursive = False

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return -1

    for i in range(1, len(argv)):
        if argv[i] == '-r':
            recursive = True
        elif filename is None:
            filename = argv[i]
        elif argv[i][0] == '-':
            print('Unexpected option : %s' % argv[i])
            return Usage()
        else:
            print('Unexpected option : %s' % argv[i])
            return Usage()

    if filename is None:
        return Usage()

    if recursive:
        ret = gdal.RmdirRecursive(filename)
    else:
        ret = gdal.Rmdir(filename)
    if ret != 0:
        print('Deletion failed')
    return ret


def main(argv):
    return gdal_rm(argv)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
