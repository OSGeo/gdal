#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
#  Project:  GDAL samples
#  Purpose:  Display the list of files in a virtual directory, like /vsicurl or /vsizip
#  Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2011-2014, Even Rouault <even dot rouault at spatialys.com>
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

import os
import stat
import sys

from osgeo import gdal


def needsVSICurl(filename):
    return filename.startswith('http://') or filename.startswith('https://') or filename.startswith('ftp://')


def iszip(filename):
    return filename.endswith('.zip') or filename.endswith('.ZIP')


def istgz(filename):
    return filename.endswith('.tgz') or filename.endswith('.TGZ') or \
        filename.endswith('.tar.gz') or filename.endswith('.TAR.GZ')


def display_file(fout, dirname, prefix, filename, longformat, check_open=False):

    statBuf = None
    filename_displayed = prefix + filename

    if dirname.endswith('/'):
        dirname_with_slash = dirname
    else:
        dirname_with_slash = dirname + '/'

    version_num = int(gdal.VersionInfo('VERSION_NUM'))
    if longformat:
        if version_num >= 1900:
            statBuf = gdal.VSIStatL(dirname_with_slash + filename, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG | gdal.VSI_STAT_SIZE_FLAG)
    else:
        if version_num >= 1900:
            statBuf = gdal.VSIStatL(dirname_with_slash + filename, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG)

    if statBuf is None and check_open:
        if version_num >= 1900:
            f = None
        else:
            f = gdal.VSIFOpenL(dirname_with_slash + filename, "rb")
        if f is None:
            sys.stderr.write('Cannot open %s\n' % (dirname_with_slash + filename))
            return
        gdal.VSIFCloseL(f)

    if statBuf is not None and statBuf.IsDirectory() and not filename_displayed.endswith('/'):
        filename_displayed = filename_displayed + "/"

    if longformat and statBuf is not None:
        import time
        bdt = time.gmtime(statBuf.mtime)
        if stat.S_IMODE(statBuf.mode) != 0:
            permissions = stat.filemode(statBuf.mode)
        elif statBuf.IsDirectory():
            permissions = "dr-xr-xr-x"
        else:
            permissions = "-r--r--r--"
        line = "%s  1 unknown unknown %12d %04d-%02d-%02d %02d:%02d %s\n" % \
            (permissions, statBuf.size, bdt.tm_year, bdt.tm_mon, bdt.tm_mday, bdt.tm_hour, bdt.tm_min, filename_displayed)
    else:
        line = filename_displayed + "\n"

    fout.write(line)


def readDir(fout, dirname, prefix, longformat, recurse, depth, recurseInZip, recurseInTGZ, first=False):

    if depth <= 0:
        return

    if needsVSICurl(dirname):
        dirname = '/vsicurl/' + dirname
        prefix = '/vsicurl/' + prefix

    if recurseInZip and iszip(dirname) and not dirname.startswith('/vsizip'):
        dirname = '/vsizip/' + dirname
        prefix = '/vsizip/' + prefix

    if recurseInTGZ and istgz(dirname) and not dirname.startswith('/vsitar'):
        dirname = '/vsitar/' + dirname
        prefix = '/vsitar/' + prefix

    lst = gdal.ReadDir(dirname)
    if lst is None:
        if first:
            original_dirname = dirname
            (dirname, filename) = os.path.split(dirname)
            if gdal.ReadDir(dirname) is None:
                sys.stderr.write('Cannot open %s\n' % original_dirname)
                return
            if dirname == '':
                dirname = '.'
                prefix = ''
            else:
                prefix = dirname + '/'
            display_file(fout, dirname, prefix, filename, longformat, True)
    else:
        for filename in lst:
            if filename == '.' or filename == '..':
                continue

            display_file(fout, dirname, prefix, filename, longformat)

            if recurse:
                new_prefix = prefix + filename
                if not new_prefix.endswith('/'):
                    new_prefix += '/'
                readDir(fout, dirname + '/' + filename, new_prefix,
                        longformat, recurse, depth - 1, recurseInZip, recurseInTGZ)


def Usage():
    print('Usage: gdal_ls [-l] [-r] [-depth d] [-Rzip] [-Rtgz] name_of_virtual_directory')
    print('')
    print('Display the list of files in a virtual directory, like /vsicurl or /vsizip')
    print('')
    print('Options :')
    print(' -l : use a long listing format (same as ls -l)')
    print(' -r : list subdirectories recursively')
    print(' -depth d : recurse until depth d')
    print(' -Rzip : list content of .zip archives')
    print(' -Rtgz : list content of .tar.gz/.tgz archives (potentially slow on /vsicurl/)')
    return -1


def gdal_ls(argv, fout=sys.stdout):
    longformat = False
    recurse = False
    recurseInZip = False
    recurseInTGZ = False
    display_prefix = True
    dirname = None
    depth = 1024

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return -1

    i = 1
    argc = len(argv)
    while i < argc:
        if argv[i] == '-l':
            longformat = True
        elif argv[i] == '-lr':
            longformat = True
            recurse = True
        elif argv[i] == '-R' or argv[i] == '-r':
            recurse = True
        elif argv[i] == '-Rzip':
            recurseInZip = True
        elif argv[i] == '-Rtgz':
            recurseInTGZ = True
        elif argv[i] == '-noprefix':
            display_prefix = False
        elif argv[i] == '-depth' and i < len(argv) - 1:
            depth = int(argv[i + 1])
            i = i + 1
        elif argv[i][0] == '-':
            sys.stderr.write('Unrecognized option : %s\n' % argv[i])
            return Usage()
        elif dirname is None:
            dirname = argv[i]
        else:
            sys.stderr.write('Unexpected option : %s\n' % argv[i])
            return Usage()

        i = i + 1

    if dirname is None:
        return Usage()

    # Remove trailing
    if dirname[-1] == '/':
        dirname = dirname[0:len(dirname) - 1]

    if needsVSICurl(dirname):
        dirname = '/vsicurl/' + dirname

    # if iszip(dirname) and not dirname.startswith('/vsizip'):
    #    dirname = '/vsizip/' + dirname

    # if istgz(dirname) and not dirname.startswith('/vsitar'):
    #    dirname = '/vsitar/' + dirname

    prefix = ''
    if display_prefix:
        prefix = dirname + '/'
    readDir(fout, dirname, prefix, longformat, recurse, depth, recurseInZip, recurseInTGZ, True)
    return 0


def main(argv=sys.argv):
    return gdal_ls(argv)


if __name__ == '__main__':
    version_num = int(gdal.VersionInfo('VERSION_NUM'))
    if version_num < 1800:
        sys.stderr.write('ERROR: Python bindings of GDAL 1.8.0 or later required\n')
        sys.exit(1)

    sys.exit(main(sys.argv))
