#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#  $Id$
#
#  Project:  GDAL
#  Purpose:  Simple command line program for copying the color table of a
#            raster into another raster.
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

from osgeo import gdal
from osgeo.auxiliary.base import GetOutputDriverFor


def Usage():
    print('Usage: attachpct.py <pctfile> <infile> <outfile>')
    return 1


def main(argv):
    if len(argv) < 3:
        return Usage()
    ct_filename = argv[1]
    src_filename = argv[2]
    dst_filename = argv[3]
    _ds, err = doit(ct_filename, src_filename, dst_filename)
    return err


def doit(pct_filename, src_filename, dst_filename, frmt=None):

    # =============================================================================
    # Get the PCT.
    # =============================================================================
    ds = gdal.Open(pct_filename)
    ct = ds.GetRasterBand(1).GetRasterColorTable()

    if ct is None:
        print('No color table on file ', pct_filename)
        return None, 1

    ct = ct.Clone()

    # =============================================================================
    # Create a MEM clone of the source file.
    # =============================================================================

    src_ds = gdal.Open(src_filename)

    mem_ds = gdal.GetDriverByName('MEM').CreateCopy('mem', src_ds)

    # =============================================================================
    # Assign the color table in memory.
    # =============================================================================

    mem_ds.GetRasterBand(1).SetRasterColorTable(ct)
    mem_ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_PaletteIndex)

    # =============================================================================
    # Write the dataset to the output file.
    # =============================================================================

    if frmt is None:
        frmt = GetOutputDriverFor(dst_filename)

    out_ds = frmt.CreateCopy(dst_filename, mem_ds)

    mem_ds = None
    src_ds = None

    return out_ds, 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
