#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL
# Purpose:  Script to translate GDAL supported raster into XYZ ASCII
#            point stream.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################
# 
#  $Log$
#  Revision 1.3  2004/04/02 17:40:44  warmerda
#  added GDALGeneralCmdLineProcessor() support
#
#  Revision 1.2  2002/09/04 18:11:17  warmerda
#  fixed to emit center of pixel
#
#  Revision 1.1  2002/09/04 17:58:07  warmerda
#  New
#
#

import gdal
import sys
import Numeric

# =============================================================================
def Usage():
    print 'Usage: gdal2xyz.py [-skip factor] [-srcwin xoff yoff width height]'
    print '                   [-band b] srcfile [dstfile]'
    print
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
    band_num = 1

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
            band_num = int(argv[i+1])
            i = i + 1

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

    # Open source file. 
    srcds = gdal.Open( srcfile )
    if srcds is None:
        print 'Could not open %s.' % srcfile
        sys.exit( 1 )

    band = srcds.GetRasterBand(band_num)
    if band is None:
        print 'Could not get band %d' % band_num
        sys.exit( 1 )

    gt = srcds.GetGeoTransform()
  
    # Collect information on all the source files.
    if srcwin is None:
        srcwin = (0,0,srcds.RasterXSize,srcds.RasterYSize)

    # Open the output file.
    if dstfile is not None:
        dst_fh = open(dstfile,'wt')
    else:
        dst_fh = sys.stdout

    # Setup an appropriate print format.
    if abs(gt[0]) < 180 and abs(gt[3]) < 180 \
       and abs(srcds.RasterXSize * gt[1]) < 180 \
       and abs(srcds.RasterYSize * gt[5]) < 180:
        format = '%.10g %.10g %g\n'
    else:
        format = '%.3f %.3f %g\n'

    # Loop emitting data.

    for y in range(srcwin[1],srcwin[1]+srcwin[3],skip):

        data = band.ReadAsArray( srcwin[0], y, srcwin[2], 1 )    
	data = Numeric.reshape( data, (srcwin[2],) )

        for x_i in range(0,srcwin[2],skip):

            x = x_i + srcwin[0]

            geo_x = gt[0] + (x+0.5) * gt[1] + (y+0.5) * gt[2]
            geo_y = gt[3] + (x+0.5) * gt[4] + (y+0.5) * gt[5]

            line = format % (float(geo_x),float(geo_y),data[x_i])

            dst_fh.write( line )


