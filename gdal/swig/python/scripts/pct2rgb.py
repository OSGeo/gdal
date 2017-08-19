#!/usr/bin/env python
#******************************************************************************
#  $Id$
#
#  Name:     pct2rgb
#  Project:  GDAL Python Interface
#  Purpose:  Utility to convert paletted images into RGB (or RGBA) images.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#
#******************************************************************************
#  Copyright (c) 2001, Frank Warmerdam
#  Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

import sys

from osgeo import gdal

try:
    progress = gdal.TermProgress_nocb
except:
    progress = gdal.TermProgress

try:
    import numpy as Numeric
    Numeric.arrayrange = Numeric.arange
except ImportError:
    import Numeric


def Usage():
    print('Usage: pct2rgb.py [-of format] [-b <band>] [-rgba] source_file dest_file')
    sys.exit(1)

# =============================================================================
# 	Mainline
# =============================================================================

format = 'GTiff'
src_filename = None
dst_filename = None
out_bands = 3
band_number = 1

gdal.AllRegister()
argv = gdal.GeneralCmdLineProcessor( sys.argv )
if argv is None:
    sys.exit( 0 )

# Parse command line arguments.
i = 1
while i < len(argv):
    arg = argv[i]

    if arg == '-of' or arg == '-f':
        i = i + 1
        format = argv[i]

    elif arg == '-b':
        i = i + 1
        band_number = int(argv[i])

    elif arg == '-rgba':
        out_bands = 4

    elif src_filename is None:
        src_filename = argv[i]

    elif dst_filename is None:
        dst_filename = argv[i]

    else:
        Usage()

    i = i + 1

if dst_filename is None:
    Usage()

# ----------------------------------------------------------------------------
# Open source file

src_ds = gdal.Open( src_filename )
if src_ds is None:
    print('Unable to open %s ' % src_filename)
    sys.exit(1)

src_band = src_ds.GetRasterBand(band_number)

# ----------------------------------------------------------------------------
# Ensure we recognise the driver.

dst_driver = gdal.GetDriverByName(format)
if dst_driver is None:
    print('"%s" driver not registered.' % format)
    sys.exit(1)

# ----------------------------------------------------------------------------
# Build color table.

ct = src_band.GetRasterColorTable()

ct_size = ct.GetCount()
lookup = [Numeric.arrayrange(ct_size),
          Numeric.arrayrange(ct_size),
          Numeric.arrayrange(ct_size),
          Numeric.ones(ct_size) * 255]

if ct is not None:
    for i in range(ct_size):
        entry = ct.GetColorEntry(i)
        for c in range(4):
            lookup[c][i] = entry[c]

# ----------------------------------------------------------------------------
# Create the working file.

if format == 'GTiff':
    tif_filename = dst_filename
else:
    tif_filename = 'temp.tif'

gtiff_driver = gdal.GetDriverByName( 'GTiff' )

tif_ds = gtiff_driver.Create( tif_filename,
                              src_ds.RasterXSize, src_ds.RasterYSize, out_bands )


# ----------------------------------------------------------------------------
# We should copy projection information and so forth at this point.

tif_ds.SetProjection( src_ds.GetProjection() )
tif_ds.SetGeoTransform( src_ds.GetGeoTransform() )
if src_ds.GetGCPCount() > 0:
    tif_ds.SetGCPs( src_ds.GetGCPs(), src_ds.GetGCPProjection() )

# ----------------------------------------------------------------------------
# Do the processing one scanline at a time.

progress( 0.0 )
for iY in range(src_ds.RasterYSize):
    src_data = src_band.ReadAsArray(0,iY,src_ds.RasterXSize,1)

    for iBand in range(out_bands):
        band_lookup = lookup[iBand]

        dst_data = Numeric.take(band_lookup,src_data)
        tif_ds.GetRasterBand(iBand+1).WriteArray(dst_data,0,iY)

    progress( (iY+1.0) / src_ds.RasterYSize )


tif_ds = None

# ----------------------------------------------------------------------------
# Translate intermediate file to output format if desired format is not TIFF.

if tif_filename != dst_filename:
    tif_ds = gdal.Open( tif_filename )
    dst_driver.CreateCopy( dst_filename, tif_ds )
    tif_ds = None

    gtiff_driver.Delete( tif_filename )
