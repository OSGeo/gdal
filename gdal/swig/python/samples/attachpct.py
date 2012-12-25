#!/usr/bin/env python
# -*- coding: utf-8 -*-
#******************************************************************************
#  $Id$
# 
#  Project:  GDAL
#  Purpose:  Simple command line program for copying the color table of a
#            raster into another raster.
#  Author:   Frank Warmerdam, warmerda@home.com
# 
#******************************************************************************
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
#******************************************************************************

try:
    from osgeo import gdal
except ImportError:
    import gdal

import sys
import string

if len(sys.argv) < 3:
    print('Usage: attachpct.py <pctfile> <infile> <outfile>')
    sys.exit(1)

# =============================================================================
# Get the PCT.
# =============================================================================
ds = gdal.Open( sys.argv[1] )
ct = ds.GetRasterBand(1).GetRasterColorTable()

if ct is None:
    print('No color table on file ', sys.argv[1])
    sys.exit(1)

ct = ct.Clone()

ds = None

# =============================================================================
# Create a MEM clone of the source file. 
# =============================================================================

src_ds = gdal.Open( sys.argv[2] )

mem_ds = gdal.GetDriverByName( 'MEM' ).CreateCopy( 'mem', src_ds )

# =============================================================================
# Assign the color table in memory.
# =============================================================================

mem_ds.GetRasterBand(1).SetRasterColorTable( ct )
mem_ds.GetRasterBand(1).SetRasterColorInterpretation( gdal.GCI_PaletteIndex )

# =============================================================================
# Write the dataset to the output file. 
# =============================================================================

drv = gdal.GetDriverByName( 'GTiff' )

out_ds = drv.CreateCopy( sys.argv[3], mem_ds )

out_ds = None
mem_ds = None
src_ds = None







