#!/usr/bin/env python
###############################################################################
# $Id$
#
#  Project:  GDAL samples
#  Purpose:  Copy a virtual file
#  Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
#  Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

from osgeo import gdal
import sys
import os
import fnmatch

def needsVSICurl(filename):
    return filename.startswith('http://') or filename.startswith('https://') or filename.startswith('ftp://')

def Usage():
    print('Usage: gdal_cp [-progress] [-r] [-skipfailures] source_file target_file')
    return -1

class TermProgress:
    def __init__(self):
        self.nLastTick = -1

    def Progress(self, dfComplete, message):

        self.nThisTick = int(dfComplete * 40.0)
        if self.nThisTick > 40:
            self.nThisTick = 40

        #// Have we started a new progress run?
        if self.nThisTick < self.nLastTick and self.nLastTick >= 39:
            self.nLastTick = -1;

        if self.nThisTick <= self.nLastTick:
            return True

        while self.nThisTick > self.nLastTick:
            self.nLastTick = self.nLastTick + 1
            if self.nLastTick % 4 == 0:
                val = int((self.nLastTick / 4) * 10)
                sys.stdout.write("%d" % val)
            else:
                sys.stdout.write(".")

        if self.nThisTick == 40:
            print( " - done." )

        sys.stdout.flush()

        return True

class ScaledProgress:
    def __init__(self, dfMin, dfMax, UnderlyingProgress):
        self.dfMin = dfMin
        self.dfMax = dfMax
        self.UnderlyingProgress = UnderlyingProgress

    def Progress(self, dfComplete, message):
        return self.UnderlyingProgress.Progress( dfComplete * (self.dfMax - self.dfMin) + self.dfMin,
                                                 message )

def gdal_cp_single(srcfile, targetfile, progress):
    try:
        if os.path.isdir(targetfile):
            (head, tail) = os.path.split(srcfile)
            targetfile = targetfile + '/' + tail
    except:
        pass

    fin = gdal.VSIFOpenL(srcfile, "rb")
    if fin is None:
        print('Cannot open %s' % srcfile)
        return -1

    fout = gdal.VSIFOpenL(targetfile, "wb")
    if fout is None:
        print('Cannot create %s' % targetfile)
        gdal.VSIFCloseL(fin)
        return -1

    version_num = int(gdal.VersionInfo('VERSION_NUM'))
    total_size = 0
    if version_num < 1900 or progress is not None:
        gdal.VSIFSeekL(fin, 0, 2)
        total_size = gdal.VSIFTellL(fin)
        gdal.VSIFSeekL(fin, 0, 0)

    buffer_max_size = 4096
    copied = 0
    ret = 0
    #print('Copying %s...' % srcfile)
    if progress is not None:
        if not progress.Progress(0.0, 'Copying %s' % srcfile):
            print('Copy stopped by user')
            ret = -2

    while ret == 0:
        if total_size != 0 and copied + buffer_max_size > total_size:
            to_read = total_size - copied
        else:
            to_read = buffer_max_size
        buffer = gdal.VSIFReadL(1, to_read, fin)
        if buffer is None:
            if copied == 0:
                print('Cannot read %d bytes in %s' % (to_read, srcfile))
                ret = -1
            break
        buffer_size = len(buffer)
        if gdal.VSIFWriteL(buffer, 1, buffer_size, fout) != buffer_size:
            print('Error writing %d bytes' % buffer_size)
            ret = -1
            break
        copied += buffer_size
        if progress is not None and total_size != 0:
            if not progress.Progress(copied * 1.0 / total_size, 'Copying %s' % srcfile):
                print('Copy stopped by user')
                ret = -2
                break
        if to_read < buffer_max_size or buffer_size != buffer_max_size:
            break

    gdal.VSIFCloseL(fin)
    gdal.VSIFCloseL(fout)

    return ret

def gdal_cp_recurse(srcdir, targetdir, progress, skip_failure):

    if srcdir[-1] == '/':
        srcdir = srcdir[0:len(srcdir)-1]
    lst = gdal.ReadDir(srcdir)
    if lst is None:
        print('%s is not a directory' % srcdir)
        return -1

    try:
        os.stat(targetdir)
    except:
        os.mkdir(targetdir)

    for srcfile in lst:
        if srcfile == '.' or srcfile == '..':
            continue
        fullsrcfile = srcdir + '/' + srcfile
        statBuf = gdal.VSIStatL(fullsrcfile, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG)
        if statBuf.IsDirectory():
            ret = gdal_cp_recurse(fullsrcfile, targetdir + '/' + srcfile, progress, skip_failure)
        else:
            ret = gdal_cp_single(fullsrcfile, targetdir, progress)
        if ret == -2 or (ret == -1 and not skip_failure):
            return ret
    return 0

def gdal_cp_pattern_match(srcdir, pattern, targetfile, progress, skip_failure):

    if srcdir == '':
        srcdir = '.'

    lst = gdal.ReadDir(srcdir)
    lst2 = []
    if lst is None:
        print('Cannot read directory %s' % srcdir)
        return -1

    for filename in lst:
        if filename == '.' or filename == '..':
            continue
        if srcdir != '.':
            lst2.append(srcdir + '/' + filename)
        else:
            lst2.append(filename)

    if progress is not None:
        total_size = 0
        filesizelst = []
        for srcfile in lst2:
            filesize = 0
            if fnmatch.fnmatch(srcfile, pattern):
                fin = gdal.VSIFOpenL(srcfile, "rb")
                if fin is not None:
                    gdal.VSIFSeekL(fin, 0, 2)
                    filesize = gdal.VSIFTellL(fin)
                    gdal.VSIFCloseL(fin)

            filesizelst.append(filesize)
            total_size = total_size + filesize

        if total_size == 0:
            return -1

        cursize = 0
        i = 0
        for srcfile in lst2:
            if filesizelst[i] != 0:
                dfMin = cursize * 1.0 / total_size
                dfMax = (cursize + filesizelst[i]) * 1.0 / total_size
                scaled_progress = ScaledProgress(dfMin, dfMax, progress)

                ret = gdal_cp_single(srcfile, targetfile, scaled_progress)
                if ret == -2 or (ret == -1 and not skip_failure):
                    return ret

                cursize += filesizelst[i]

            i = i + 1

    else:
        for srcfile in lst2:
            if fnmatch.fnmatch(srcfile, pattern):
                ret = gdal_cp_single(srcfile, targetfile, progress)
                if ret == -2 or (ret == -1 and not skip_failure):
                    return ret
    return 0


def gdal_cp(argv, progress = None):
    srcfile = None
    targetfile = None
    recurse = False
    skip_failure = False

    argv = gdal.GeneralCmdLineProcessor( argv )
    if argv is None:
        return -1

    for i in range(1, len(argv)):
        if argv[i] == '-progress':
            progress = TermProgress()
        elif argv[i] == '-r':
            version_num = int(gdal.VersionInfo('VERSION_NUM'))
            if version_num < 1900:
                print('ERROR: Python bindings of GDAL 1.9.0 or later required for -r option')
                return -1
            recurse = True
        elif len(argv[i]) >= 5 and argv[i][0:5] == '-skip':
            skip_failure = True
        elif argv[i][0] == '-':
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

    if needsVSICurl(srcfile):
        srcfile = '/vsicurl/' + srcfile

    if recurse:
        # Make sure that 'gdal_cp.py -r [srcdir/]lastsubdir targetdir' creates
        # targetdir/lastsubdir if targetdir already exists (like cp -r does).
        if srcfile[-1] == '/':
            srcfile = srcfile[0:len(srcfile)-1]
        statBufSrc = gdal.VSIStatL(srcfile, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG)
        statBufDst = gdal.VSIStatL(targetfile, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG)
        if statBufSrc is not None and statBufSrc.IsDirectory() and statBufDst is not None and statBufDst.IsDirectory():
            if targetfile[-1] != '/':
                if srcfile.rfind('/') != -1:
                    targetfile = targetfile + srcfile[srcfile.rfind('/'):]
                else:
                    targetfile = targetfile + '/' + srcfile
                try:
                    os.stat(targetfile)
                except:
                    os.mkdir(targetfile)
        return gdal_cp_recurse(srcfile, targetfile, progress, skip_failure)

    (srcdir, pattern) = os.path.split(srcfile)
    if pattern.find('*') != -1 or pattern.find('?') != -1:
        return gdal_cp_pattern_match(srcdir, pattern, targetfile, progress, skip_failure)
    else:
        return gdal_cp_single(srcfile, targetfile, progress)

if __name__ == '__main__':
    version_num = int(gdal.VersionInfo('VERSION_NUM'))
    if version_num < 1800:
        print('ERROR: Python bindings of GDAL 1.8.0 or later required')
        sys.exit(1)

    sys.exit(gdal_cp(sys.argv))
