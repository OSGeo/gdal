#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  InSAR Peppers
# Purpose:  Module to extract data from many rasters into one output.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2000, Atlantis Scientific Inc. (www.atlsci.com)
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
#  Revision 1.1  2005/05/23 07:31:02  fwarmerdam
#  New
#

import gdal
import sys

# =============================================================================
def Usage():
    print 'Usage: histrep.py [-force] input_file'
    print '   or'
    print '       histrep.py -req <min> <max> <buckets> [-force] [-approxok]'
    print '                  [-ioor] input_file'
    print
    sys.exit( 1 )


# =============================================================================
if __name__ == '__main__':
    argv = gdal.GeneralCmdLineProcessor( sys.argv )

    req = None
    force = 0
    approxok = 0
    ioor = 0

    file = None

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-req':
            req = (float(argv[i+1]), float(argv[i+2]), int(argv[i+3]))
            i = i + 3

        elif arg == '-ioor':
            ioor = 1

        elif arg == '-approxok':
            approxok = 1

        elif arg == '-force':
            force = 1

        elif file is None:
            file = arg

        else:
            Usage()
            
        i = i + 1

    if file is None:
        Usage()

    # -----------------------------------------------------------------------
    ds = gdal.Open( file )

    if req is None:
        hist = ds.GetRasterBand(1).GetDefaultHistogram( force=force )

        if hist is None:
            print 'No default histogram.'
        else:
            print 'Default Histogram:' 
            print 'Min: ', hist[0]
            print 'Max: ', hist[1]
            print 'Buckets: ', hist[2]
            print 'Histogram: ', hist[3]
        
    else:
        hist = ds.GetRasterBand(1).GetHistogram( req[0], req[1], req[2],
                                                ioor, approxok )

        if hist is not None:
            print 'Histogram: ', hist

    ds = None
