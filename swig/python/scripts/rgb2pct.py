#!/usr/bin/env python
#******************************************************************************
#  $Id$
# 
#  Name:     rgb2pct
#  Project:  GDAL Python Interface
#  Purpose:  Application for converting an RGB image to a pseudocolored image.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
# 
#******************************************************************************
#  Copyright (c) 2001, Frank Warmerdam
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
import os.path

def Usage():
    print('Usage: rgb2pct.py [-n colors | -pct palette_file] [-of format] source_file dest_file')
    sys.exit(1)

# =============================================================================
#      Mainline
# =============================================================================

color_count = 256
format = 'GTiff'
src_filename = None
dst_filename = None
pct_filename = None

gdal.AllRegister()
argv = gdal.GeneralCmdLineProcessor( sys.argv )
if argv is None:
    sys.exit( 0 )

# Parse command line arguments.
i = 1
while i < len(argv):
    arg = argv[i]

    if arg == '-of':
        i = i + 1
        format = argv[i]

    elif arg == '-n':
        i = i + 1
        color_count = int(argv[i])

    elif arg == '-pct':
        i = i + 1
        pct_filename = argv[i]

    elif src_filename is None:
        src_filename = argv[i]

    elif dst_filename is None:
        dst_filename = argv[i]

    else:
        Usage()

    i = i + 1

if dst_filename is None:
    Usage()
    
# Open source file

src_ds = gdal.Open( src_filename )
if src_ds is None:
    print('Unable to open %s' % src_filename)
    sys.exit(1)

if src_ds.RasterCount < 3:
    print('%s has %d band(s), need 3 for inputs red, green and blue.' \
          % (src_filename, src_ds.RasterCount))
    sys.exit(1)

# Ensure we recognise the driver.

dst_driver = gdal.GetDriverByName(format)
if dst_driver is None:
    print('"%s" driver not registered.' % format)
    sys.exit(1)

# Generate palette

ct = gdal.ColorTable()
if pct_filename is None:
    err = gdal.ComputeMedianCutPCT( src_ds.GetRasterBand(1),
                                    src_ds.GetRasterBand(2),
                                    src_ds.GetRasterBand(3),
                                    color_count, ct,
                                    callback = gdal.TermProgress,
                                    callback_data = 'Generate PCT' )
else:
    pct_ds = gdal.Open(pct_filename)
    ct = pct_ds.GetRasterBand(1).GetRasterColorTable().Clone()

# Create the working file.  We have to use TIFF since there are few formats
# that allow setting the color table after creation.

if format == 'GTiff':
    tif_filename = dst_filename
else:
    tif_filename = 'temp.tif'

gtiff_driver = gdal.GetDriverByName( 'GTiff' )

tif_ds = gtiff_driver.Create( tif_filename,
                              src_ds.RasterXSize, src_ds.RasterYSize, 1)

tif_ds.GetRasterBand(1).SetRasterColorTable( ct )

# ----------------------------------------------------------------------------
# We should copy projection information and so forth at this point.

tif_ds.SetProjection( src_ds.GetProjection() )
tif_ds.SetGeoTransform( src_ds.GetGeoTransform() )
if src_ds.GetGCPCount() > 0:
    tif_ds.SetGCPs( src_ds.GetGCPs(), src_ds.GetGCPProjection() )

# ----------------------------------------------------------------------------
# Actually transfer and dither the data.

err = gdal.DitherRGB2PCT( src_ds.GetRasterBand(1),
                          src_ds.GetRasterBand(2),
                          src_ds.GetRasterBand(3),
                          tif_ds.GetRasterBand(1),
                          ct,
                          callback = gdal.TermProgress,
                          callback_data = 'Generate PCT' )

tif_ds = None

if tif_filename != dst_filename:
    tif_ds = gdal.Open( tif_filename )
    dst_driver.CreateCopy( dst_filename, tif_ds )
    tif_ds = None

    gtiff_driver.Delete( tif_filename )

