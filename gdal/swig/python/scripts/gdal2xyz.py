#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL
# Purpose:  Script to translate GDAL supported raster into XYZ ASCII
#           point stream.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

try:
    from osgeo import gdal
except ImportError:
    import gdal

import sys

try:
    import numpy as Numeric
except ImportError:
    import Numeric

# =============================================================================
def Usage():
    print('Usage: gdal2xyz.py [-skip factor] [-srcwin xoff yoff width height]')
    print('                   [-band b] [-csv] srcfile [dstfile]')
    print('')
    sys.exit( 1 )

# =============================================================================
#
# Program mainline.
#

if __name__ == '__main__':

    srcwin = None
    skip = 1    
    srcfile = None
    dstfile = None
    band_nums = []
    delim = ' '

    gdal.AllRegister()
    argv = gdal.GeneralCmdLineProcessor( sys.argv )
    if argv is None:
        sys.exit( 0 )

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-srcwin':
            srcwin = (int(argv[i+1]),int(argv[i+2]),
                      int(argv[i+3]),int(argv[i+4]))
            i = i + 4

        elif arg == '-skip':
            skip = int(argv[i+1])
            i = i + 1

        elif arg == '-band':
            band_nums.append( int(argv[i+1]) )
            i = i + 1

        elif arg == '-csv':
            delim = ','

        elif arg[0] == '-':
            Usage()

        elif srcfile is None:
            srcfile = arg

        elif dstfile is None:
            dstfile = arg

        else:
            Usage()

        i = i + 1

    if srcfile is None:
        Usage()

    if band_nums == []: band_nums = [1]
    # Open source file. 
    srcds = gdal.Open( srcfile )
    if srcds is None:
        print('Could not open %s.' % srcfile)
        sys.exit( 1 )

    bands = []
    for band_num in band_nums: 
        band = srcds.GetRasterBand(band_num)
        if band is None:
            print('Could not get band %d' % band_num)
            sys.exit( 1 )
        bands.append(band)

    gt = srcds.GetGeoTransform()
  
    # Collect information on all the source files.
    if srcwin is None:
        srcwin = (0,0,srcds.RasterXSize,srcds.RasterYSize)

    # Open the output file.
    if dstfile is not None:
        dst_fh = open(dstfile,'wt')
    else:
        dst_fh = sys.stdout

    band_format = (("%g" + delim) * len(bands)).rstrip(delim) + '\n'

    # Setup an appropriate print format.
    if abs(gt[0]) < 180 and abs(gt[3]) < 180 \
       and abs(srcds.RasterXSize * gt[1]) < 180 \
       and abs(srcds.RasterYSize * gt[5]) < 180:
        format = '%.10g' + delim + '%.10g' + delim + '%s'
    else:
        format = '%.3f' + delim + '%.3f' + delim + '%s'

    # Loop emitting data.

    for y in range(srcwin[1],srcwin[1]+srcwin[3],skip):

        data = []
        for band in bands:

            band_data = band.ReadAsArray( srcwin[0], y, srcwin[2], 1 )    
            band_data = Numeric.reshape( band_data, (srcwin[2],) )
            data.append(band_data)

        for x_i in range(0,srcwin[2],skip):

            x = x_i + srcwin[0]

            geo_x = gt[0] + (x+0.5) * gt[1] + (y+0.5) * gt[2]
            geo_y = gt[3] + (x+0.5) * gt[4] + (y+0.5) * gt[5]

            x_i_data = []
            for i in range(len(bands)):
                x_i_data.append(data[i][x_i])
            
            band_str = band_format % tuple(x_i_data)

            line = format % (float(geo_x),float(geo_y), band_str)

            dst_fh.write( line )


