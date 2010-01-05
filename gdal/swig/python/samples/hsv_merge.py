#!/usr/bin/env python
#******************************************************************************
#  $Id$
# 
#  Project:  GDAL Python Interface
#  Purpose:  Script to merge greyscale as intensity into an RGB image, for
#            instance to apply hillshading to a dem colour relief.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
# 
#******************************************************************************
#  Copyright (c) 2009, Frank Warmerdam
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

from osgeo import gdal, gdal_array
import numpy
import sys

# =============================================================================
# hsv_to_rgb()
#									
# rgb comes in as [r,g,b] with values in the range [0,255].  The returned
# hsv values will be with hue and saturation in the range [0,1] and value
# in the range [0,255]
#
def rgb_to_hsv( rgb ):

    r = rgb[0]
    g = rgb[1]
    b = rgb[2]

    maxc = numpy.maximum(r,numpy.maximum(g,b))
    minc = numpy.minimum(r,numpy.minimum(g,b))

    v = maxc

    minc_eq_maxc = numpy.equal(minc,maxc)

    # compute the difference, but reset zeros to ones to avoid divide by zeros later.
    ones = numpy.ones((r.shape[0],r.shape[1]))
    maxc_minus_minc = numpy.choose( minc_eq_maxc, (ones, maxc-minc) )

    s = (maxc-minc) / numpy.maximum(ones,maxc)
    rc = (maxc-r) / maxc_minus_minc
    gc = (maxc-g) / maxc_minus_minc
    bc = (maxc-b) / maxc_minus_minc

    maxc_is_r = numpy.equal(maxc,r)
    maxc_is_g = numpy.equal(maxc,g)
    maxc_is_b = numpy.equal(maxc,b)

    h = numpy.zeros((r.shape[0],r.shape[1]))
    h = numpy.choose( maxc_is_b, (h,4.0+gc-rc) )
    h = numpy.choose( maxc_is_g, (h,2.0+rc-bc) )
    h = numpy.choose( maxc_is_r, (h,bc-gc) )

    h = numpy.mod(h/6.0,1.0)

    hsv = numpy.asarray([h,s,v])
    
    return hsv

# =============================================================================
# hsv_to_rgb()
#									
# hsv comes in as [h,s,v] with hue and saturation in the range [0,1],
# but value in the range [0,255].

def hsv_to_rgb( hsv ):

    h = hsv[0]
    s = hsv[1]
    v = hsv[2]
    

    #if s == 0.0: return v, v, v
    i = (h*6.0).astype(int)
    f = (h*6.0) - i
    p = v*(1.0 - s)
    q = v*(1.0 - s*f)
    t = v*(1.0 - s*(1.0-f))

    r = i.choose( v, q, p, p, t, v )
    g = i.choose( t, v, v, q, p, p )
    b = i.choose( p, p, t, v, v, q )

    rgb = numpy.asarray([r,g,b]).astype(numpy.byte)
    
    return rgb

# =============================================================================
# Usage()

def Usage():
    print("""
hsv_merge.py src_rgb src_greyscale dst_rgb
""")
    sys.exit(1)
    
# =============================================================================
# 	Mainline
# =============================================================================

argv = gdal.GeneralCmdLineProcessor( sys.argv )
if argv is None:
    sys.exit( 0 )

if len(argv) != 4:
    Usage()
    
src_rgb_filename = argv[1]
src_greyscale_filename = argv[2]
dst_rgb_filename = argv[3]

print( 'Read Source RGB: %s' % src_rgb_filename )

src_rgb_ds = gdal.Open( src_rgb_filename )
src_rgb = src_rgb_ds.ReadAsArray()

if src_rgb.ndim != 3 or src_rgb.shape[0] != 3:
    print 'Source image does not appear to have three bands as required.'
    sys.exit(1)

print( 'Read Source Greyscale: %s' % src_greyscale_filename )
gs_ds = gdal.Open( src_greyscale_filename )
src_greyscale = gs_ds.GetRasterBand(1).ReadAsArray()
gs_ds = None

print( 'Converting to HSV.' )
hsv = rgb_to_hsv( src_rgb )

print( 'Substitute Greyscale for V.' )
hsv_adjusted = numpy.asarray( [hsv[0], hsv[1], src_greyscale] )

print( 'Converting back to RGB.' )
dst_rgb = hsv_to_rgb( hsv_adjusted )

print( 'Saving result: %s' % dst_rgb_filename )
gdal_array.SaveArray( dst_rgb, dst_rgb_filename, 'GTiff', src_rgb_ds )
