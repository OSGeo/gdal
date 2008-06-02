#!/usr/bin/env python
#******************************************************************************
#  $Id$
# 
#  Project:  GDAL
#  Purpose:  Convert GCPs to a point layer.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
# 
#******************************************************************************
#  Copyright (c) 20058, Howard Butler <hobu.inc@gmail.com>
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
    from osgeo import osr
    from osgeo import ogr
except ImportError:
    import osr
    import ogr

import sys

from optparse import OptionParser
usage = "usage: %prog [options] arg"
parser = OptionParser(usage)

parser.add_option("-i", "--input", dest="input",
                  help="OGR input data source", metavar="INPUT")
parser.add_option("-q", "--quiet",
                  action="store_false", dest="verbose", default=False,
                  help="don't print status messages to stdout")
parser.add_option("-o", "--output", dest='output',
                  help="OGR output data source", metavar="OUTPUT")
parser.add_option("-r", "--remainder", dest="remainder",
                  type="choice",
                  help="""what to do with the remainder -- place it at the beginning, place it at the end, or evenly distribute it across the segment""",
                  default="end", choices=['end','begin','uniform'])

def main():
    (options, args) = parser.parse_args()
if __name__=='__main__':
    main()
