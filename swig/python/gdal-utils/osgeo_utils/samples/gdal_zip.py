#!/usr/bin/env python3
###############################################################################
# $Id$
#
#  Project:  GDAL samples
#  Purpose:  Create a zip file
#  Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
#  Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
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
    print('Usage: gdal_zip [-r] zip_filename source_file*')
    return -1


def copy_file(srcfile, targetfile, recurse):

    if recurse:
        subfiles = gdal.ReadDir(srcfile)
        if subfiles:
            for subfile in subfiles:
                if subfile != '.' and subfile != '..' and \
                   not copy_file(srcfile + '/' + subfile, targetfile, True):
                    return False
            return True

    fin = gdal.VSIFOpenL(srcfile, 'rb')
    if not fin:
        print('Cannot open ' + srcfile)
        return False

    fout = gdal.VSIFOpenL(targetfile + '/' + srcfile, 'wb')
    if not fout:
        print('Cannot create ' + targetfile + '/' + srcfile)
        gdal.VSIFCloseL(fin)
        return False

    buf_max_size = 4096
    ret = True
    copied = 0
    while True:
        buf = gdal.VSIFReadL(1, buf_max_size, fin)
        if buf is None:
            if copied == 0:
                print('Cannot read from%s' % srcfile)
                ret = False
            break
        buf_size = len(buf)
        if gdal.VSIFWriteL(buf, 1, buf_size, fout) != buf_size:
            print('Error writing %d bytes' % buf_size)
            ret = False
            break
        copied += buf_size
        if buf_size != buf_max_size:
            break

    gdal.VSIFCloseL(fin)
    gdal.VSIFCloseL(fout)
    return ret


def gdal_zip(argv, progress=None):
    srcfiles = []
    targetfile = None
    recurse = False

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return -1

    if gdal.GetConfigOption('GDAL_NUM_THREADS') is None:
        gdal.SetConfigOption('GDAL_NUM_THREADS', 'ALL_CPUS')

    for i in range(1, len(argv)):
        if argv[i] == '-r':
            recurse = True
        elif argv[i][0] == '-':
            print('Unrecognized option : %s' % argv[i])
            return Usage()
        elif targetfile is None:
            targetfile = argv[i]
        else:
            srcfiles.append(argv[i])

    if not srcfiles or targetfile is None:
        return Usage()

    if not targetfile.endswith('.zip'):
        targetfile += '.zip'
    targetfile = '/vsizip/' + targetfile

    ret = 0
    for srcfile in srcfiles:
        if not copy_file(srcfile, targetfile, recurse):
            ret = 1
            break

    return ret


def main(argv=sys.argv):
    return gdal_zip(argv)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
