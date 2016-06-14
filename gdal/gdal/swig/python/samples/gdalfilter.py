#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  OGR Python samples
# Purpose:  Filter an input file, producing an output file.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

from osgeo import gdal
gdal.TermProgress = gdal.TermProgress_nocb

import sys
import string

def Usage():
    print('Usage: gdalfilter.py [-n] [-size n] [-coefs ...] [-f format] [-co NAME=VALUE]\n' \
          '                     in_file out_file')
    sys.exit(1)

# =============================================================================
# 	Mainline
# =============================================================================

srcwin = None
bands = []

srcfile = None
dstfile = None
size = 3
coefs = None
normalized = 0

out_format = None
create_options = []

# Parse command line arguments.
i = 1
while i < len(sys.argv):
    arg = sys.argv[i]

    if arg == '-size':
        size = int(sys.argv[i+1])
        i = i + 1

    elif arg == '-n':
        normalized = 1

    elif arg == '-f':
        out_format = int(sys.argv[i+1])
        i = i + 1

    elif arg == '-co':
        create_options.append(sys.argv[i+1])
        i = i + 1

    elif arg == '-coefs':
        coefs = []
        for iCoef in range(size*size):
            try:
                coefs.append( float(sys.argv[iCoef+i+1]) )
            except:
                print("Didn't find enough valid kernel coefficients, need ", \
                      size*size)
                sys.exit(1)
        i = i + size*size

    elif srcfile is None:
        srcfile = sys.argv[i]

    elif dstfile is None:
        dstfile = sys.argv[i]

    else:
        Usage()

    i = i + 1

if dstfile is None:
    Usage()

if out_format is None and string.lower(dstfile[-4:]) == '.vrt':
    out_format = 'VRT'
else:
    out_format = 'GTiff'

# =============================================================================
#   Open input file.
# =============================================================================

src_ds = gdal.Open( srcfile )

# =============================================================================
#   Create a virtual file in memory only which matches the configuration of
#   the input file.
# =============================================================================

vrt_driver = gdal.GetDriverByName( 'VRT' )
vrt_ds = vrt_driver.CreateCopy( '', src_ds )

# =============================================================================
#   Prepare coefficient list.
# =============================================================================
coef_list_size = size * size

if coefs is None:
    coefs = []
    for i in range(coef_list_size):
        coefs.append( 1.0 / coef_list_size )

coefs_string = ''
for i in range(coef_list_size):
    coefs_string = coefs_string + ('%.8g ' % coefs[i])

# =============================================================================
#   Prepare template for XML description of the filtered source.
# =============================================================================

filt_template = \
'''<KernelFilteredSource>
  <SourceFilename>%s</SourceFilename>
  <SourceBand>%%d</SourceBand>
  <Kernel normalized="%d">
    <Size>%d</Size>
    <Coefs>%s</Coefs>
  </Kernel>
</KernelFilteredSource>''' % (srcfile, normalized, size, coefs_string)

# =============================================================================
#	Go through all the bands replacing the SimpleSource with a filtered
#       source.
# =============================================================================

for iBand in range(vrt_ds.RasterCount):
    band = vrt_ds.GetRasterBand(iBand+1)

    src_xml = filt_template % (iBand+1)

    band.SetMetadata( { 'source_0' : src_xml }, 'vrt_sources' )

# =============================================================================
#	copy the results to a new file.
# =============================================================================

if out_format == 'VRT':
    vrt_ds.SetDescription( dstfile )
    vrt_ds = None
    sys.exit(0)

out_driver = gdal.GetDriverByName( out_format )
if out_driver is None:
    print('Output driver %s does not appear to exist.' % out_format)
    sys.exit(1)

out_ds = out_driver.CreateCopy( dstfile, vrt_ds, options = create_options,
                                callback = gdal.TermProgress )
out_ds = None






