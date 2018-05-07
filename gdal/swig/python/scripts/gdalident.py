#!/usr/bin/env python
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL
#  Purpose:  Application to identify files by format.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
# ******************************************************************************
#  Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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
#

import os
import stat
import sys

from osgeo import gdal


# =============================================================================
# 	Usage()
# =============================================================================
def Usage():
    print('Usage: gdalident.py [-r] file(s)')
    sys.exit(1)

# =============================================================================
# 	ProcessTarget()
# =============================================================================


def ProcessTarget(target, recursive, report_failure, filelist=None):

    if filelist is not None:
        driver = gdal.IdentifyDriver(target, filelist)
    else:
        driver = gdal.IdentifyDriver(target)

    if driver is not None:
        print('%s: %s' % (target, driver.ShortName))
    elif report_failure:
        print('%s: unrecognized' % target)

    if recursive and driver is None:
        try:
            mode = os.stat(target)[stat.ST_MODE]
        except OSError:
            mode = 0

        if stat.S_ISDIR(mode):
            subfilelist = os.listdir(target)
            for item in subfilelist:
                subtarget = os.path.join(target, item)
                ProcessTarget(subtarget, 1, report_failure, subfilelist)

# =============================================================================
# 	Mainline
# =============================================================================


recursive = 0
report_failure = 0
files = []

gdal.AllRegister()
argv = gdal.GeneralCmdLineProcessor(sys.argv)
if argv is None:
    sys.exit(0)

# Parse command line arguments.
i = 1
while i < len(argv):
    arg = argv[i]

    if arg == '-r':
        recursive = 1

    elif arg == '-f':
        report_failure = 1

    else:
        files.append(arg)

    i = i + 1

if not files:
    Usage()

for f in files:
    ProcessTarget(f, recursive, report_failure)
