#!/usr/bin/env python3
###############################################################################
# $Id$
#
#  Project:  GDAL samples
#  Purpose:  Copy a virtual file
#  Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

import fnmatch
import os
import stat
import sys

from osgeo import gdal


def needsVSICurl(filename):
    return (
        filename.startswith("http://")
        or filename.startswith("https://")
        or filename.startswith("ftp://")
    )


def Usage():
    print("Usage: gdal_cp [-progress] [-r] [-skipfailures] source_file target_file")
    return 2


class ScaledProgress(object):
    def __init__(self, dfMin, dfMax, UnderlyingProgress):
        self.dfMin = dfMin
        self.dfMax = dfMax
        self.UnderlyingProgress = UnderlyingProgress

    def Progress(self, dfComplete, message, user_data):
        return self.UnderlyingProgress(
            dfComplete * (self.dfMax - self.dfMin) + self.dfMin, message, user_data
        )


def gdal_cp_single(srcfile, targetfile, progress):
    if targetfile.endswith("/"):
        stat_res = gdal.VSIStatL(targetfile)
    else:
        stat_res = gdal.VSIStatL(targetfile + "/")

    if (stat_res is None and targetfile.endswith("/")) or (
        stat_res is not None and stat.S_ISDIR(stat_res.mode)
    ):
        (_, tail) = os.path.split(srcfile)
        if targetfile.endswith("/"):
            targetfile = targetfile + tail
        else:
            targetfile = targetfile + "/" + tail

    fin = gdal.VSIFOpenL(srcfile, "rb")
    if fin is None:
        print("Cannot open %s" % srcfile)
        return -1

    ret = gdal.CopyFile(srcfile, targetfile, fin, callback=progress)

    gdal.VSIFCloseL(fin)

    return ret


def gdal_cp_recurse(srcdir, targetdir, progress, skip_failure):

    if srcdir[-1] == "/":
        srcdir = srcdir[0 : len(srcdir) - 1]
    lst = gdal.ReadDir(srcdir)
    if lst is None:
        print("%s is not a directory" % srcdir)
        return -1

    if gdal.VSIStatL(targetdir) is None:
        gdal.Mkdir(targetdir, int("0755", 8))

    for srcfile in lst:
        if srcfile == "." or srcfile == "..":
            continue
        fullsrcfile = srcdir + "/" + srcfile
        statBuf = gdal.VSIStatL(
            fullsrcfile, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG
        )
        if statBuf.IsDirectory():
            ret = gdal_cp_recurse(
                fullsrcfile, targetdir + "/" + srcfile, progress, skip_failure
            )
        else:
            ret = gdal_cp_single(fullsrcfile, targetdir, progress)
        if ret == -2 or (ret == -1 and not skip_failure):
            return ret
    return 0


def gdal_cp_pattern_match(srcdir, pattern, targetfile, progress, skip_failure):

    if srcdir == "":
        srcdir = "."

    lst = gdal.ReadDir(srcdir)
    lst2 = []
    if lst is None:
        print("Cannot read directory %s" % srcdir)
        return -1

    for filename in lst:
        if filename == "." or filename == "..":
            continue
        if srcdir != ".":
            lst2.append(srcdir + "/" + filename)
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

                ret = gdal_cp_single(srcfile, targetfile, scaled_progress.Progress)
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


def gdal_cp(argv, progress=None):
    srcfile = None
    targetfile = None
    recurse = False
    skip_failure = False

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return -1

    for i in range(1, len(argv)):
        if argv[i] == "-progress":
            progress = gdal.TermProgress_nocb
        elif argv[i] == "-r":
            recurse = True
        elif len(argv[i]) >= 5 and argv[i][0:5] == "-skip":
            skip_failure = True
        elif argv[i][0] == "-":
            print("Unrecognized option : %s" % argv[i])
            return Usage()
        elif srcfile is None:
            srcfile = argv[i]
        elif targetfile is None:
            targetfile = argv[i]
        else:
            print("Unexpected option : %s" % argv[i])
            return Usage()

    if srcfile is None or targetfile is None:
        return Usage()

    if needsVSICurl(srcfile):
        srcfile = "/vsicurl/" + srcfile

    if recurse:
        # Make sure that 'gdal_cp.py -r [srcdir/]lastsubdir targetdir' creates
        # targetdir/lastsubdir if targetdir already exists (like cp -r does).
        if srcfile[-1] == "/":
            srcfile = srcfile[0 : len(srcfile) - 1]
        statBufSrc = gdal.VSIStatL(
            srcfile, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG
        )
        if statBufSrc is None:
            statBufSrc = gdal.VSIStatL(
                srcfile + "/", gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG
            )
        statBufDst = gdal.VSIStatL(
            targetfile, gdal.VSI_STAT_EXISTS_FLAG | gdal.VSI_STAT_NATURE_FLAG
        )
        if (
            statBufSrc is not None
            and statBufSrc.IsDirectory()
            and statBufDst is not None
            and statBufDst.IsDirectory()
        ):
            if targetfile[-1] == "/":
                targetfile = targetfile[0:-1]
            if srcfile.rfind("/") != -1:
                targetfile = targetfile + srcfile[srcfile.rfind("/") :]
            else:
                targetfile = targetfile + "/" + srcfile

            if gdal.VSIStatL(targetfile) is None:
                gdal.Mkdir(targetfile, int("0755", 8))

        return gdal_cp_recurse(srcfile, targetfile, progress, skip_failure)

    (srcdir, pattern) = os.path.split(srcfile)
    if not srcdir.startswith("/vsi") and ("*" in pattern or "?" in pattern):
        return gdal_cp_pattern_match(
            srcdir, pattern, targetfile, progress, skip_failure
        )
    return gdal_cp_single(srcfile, targetfile, progress)


def main(argv=sys.argv):
    return gdal_cp(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
