#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test "random" TIFF files
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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
import random

COMPRESSION_NONE = 1
PHOTOMETRIC_MINISBLACK = 1
PLANARCONFIG_CONTIG = 1
PLANARCONFIG_SEPARATE = 2
SAMPLEFORMAT_UINT = 1

TIFFTAG_IMAGEWIDTH = 256
TIFFTAG_IMAGELENGTH = 257
TIFFTAG_BITSPERSAMPLE = 258
TIFFTAG_COMPRESSION = 259
TIFFTAG_PHOTOMETRIC = 262
TIFFTAG_STRIPOFFSETS = 273
TIFFTAG_SAMPLESPERPIXEL = 277
TIFFTAG_ROWSPERSTRIP = 278
TIFFTAG_STRIPBYTECOUNTS = 279
TIFFTAG_PLANARCONFIG = 284
TIFFTAG_TILEWIDTH = 322
TIFFTAG_TILELENGTH = 323
TIFFTAG_TILEOFFSETS = 324
TIFFTAG_TILEBYTECOUNTS = 325
TIFFTAG_SAMPLEFORMAT = 339

tags = [ ['TIFFTAG_IMAGEWIDTH', TIFFTAG_IMAGEWIDTH, [0, 1, 0x7FFFFFFF, 0xFFFFFFFF]],
         ['TIFFTAG_IMAGELENGTH', TIFFTAG_IMAGELENGTH, [0, 1, 0x7FFFFFFF, 0xFFFFFFFF]],
         ['TIFFTAG_BITSPERSAMPLE', TIFFTAG_BITSPERSAMPLE, [None, 1, 8, 255]],
         #['TIFFTAG_COMPRESSION', TIFFTAG_COMPRESSION, [None, COMPRESSION_NONE]],
         #['TIFFTAG_PHOTOMETRIC', TIFFTAG_PHOTOMETRIC, [None, PHOTOMETRIC_MINISBLACK]],
         ['TIFFTAG_STRIPOFFSETS', TIFFTAG_STRIPOFFSETS, [None, 0, 8]],
         ['TIFFTAG_SAMPLESPERPIXEL', TIFFTAG_SAMPLESPERPIXEL, [None, 1, 255, 65535]],
         ['TIFFTAG_ROWSPERSTRIP', TIFFTAG_ROWSPERSTRIP, [None, 0, 1, 0x7FFFFFFF, 0xFFFFFFFF]],
         ['TIFFTAG_STRIPBYTECOUNTS', TIFFTAG_STRIPBYTECOUNTS, [None, 0,1,0x7FFFFFFF,0xFFFFFFFF]],
         ['TIFFTAG_PLANARCONFIG', TIFFTAG_PLANARCONFIG, [None, PLANARCONFIG_CONTIG, PLANARCONFIG_SEPARATE]],
         ['TIFFTAG_TILEWIDTH', TIFFTAG_TILEWIDTH, [None, 0, 8, 256, 65536, 0x7FFFFFFF]],
         ['TIFFTAG_TILELENGTH', TIFFTAG_TILELENGTH, [None, 0, 8, 256, 65536, 0x7FFFFFFF]],
         ['TIFFTAG_TILEOFFSETS', TIFFTAG_TILEOFFSETS, [None, 0, 8]],
         ['TIFFTAG_TILEBYTECOUNTS', TIFFTAG_TILEBYTECOUNTS, [None, 0, 1, 0x7FFFFFFF, 0xFFFFFFFF]],
         #['TIFFTAG_SAMPLEFORMAT', TIFFTAG_SAMPLEFORMAT, [None, SAMPLEFORMAT_UINT]],
]

def generate_tif(comb_val):
    idx_tab = []
    count_non_none = 0
    has_strip = False
    for level in range(len(tags)):
        tag = tags[level][1]
        possible_vals = tags[level][2]
        len_possible_vals = tags[level][3]
        idx_val = comb_val % len_possible_vals
        comb_val = int(comb_val / len_possible_vals)
        val = possible_vals[idx_val]

        if tag == TIFFTAG_TILEWIDTH or \
           tag == TIFFTAG_TILELENGTH or \
           tag == TIFFTAG_TILEOFFSETS or \
           tag == TIFFTAG_TILEBYTECOUNTS:
               if has_strip:
                   idx_val = 0
                   val = None
        idx_tab.append(idx_val)

        if val is not None:
            if tag == TIFFTAG_STRIPOFFSETS or \
               tag == TIFFTAG_ROWSPERSTRIP or \
               tag == TIFFTAG_STRIPBYTECOUNTS:
                has_strip = True
            count_non_none = count_non_none + 1
        #    print('%s : %d' % (tags[level][0], val))
        #else:
        #    print('%s : None' % (tags[level][0]))

    content = '\x49\x49\x2A\x00\x08\x00\x00\x00' + ('%c' % count_non_none) + '\x00'
    for level in range(len(tags)):
        tag = tags[level][1]
        possible_vals = tags[level][2]
        idx = idx_tab[level]
        val = possible_vals[idx]
        if val is not None:
            content = content + ('%c' % (tag & 255)) + ('%c' % (tag >> 8))
            content = content + '\x04\x00\x01\x00\x00\x00'
            content = content + ('%c' % (val & 255)) + ('%c' % ((val >> 8)& 255)) + ('%c' % ((val >> 16)& 255)) + ('%c' % ((val >> 24) & 255))

    return content

nVals = 1
for level in range(len(tags)):
    len_possible_vals = len(tags[level][2])
    tags[level].append(len_possible_vals)
    nVals = nVals * len_possible_vals
iter = 0
while True:
    iter = iter + 1
    comb_val = random.randrange(0, nVals)
    content = generate_tif(comb_val)

    #f = open('test.tif', 'wb')
    #f.write(content)
    #f.close()

    #print(struct.unpack('B' * len(content), content))

    # Create in-memory file
    gdal.FileFromMemBuffer('/vsimem/test.tif', content)

    print('iter = %d, comb_val_init = %d' % (iter, comb_val))
    ds = gdal.Open('/vsimem/test.tif')
    #if ds is not None and ds.RasterCount == 1:
    #    (blockxsize, blockysize) = ds.GetRasterBand(1).GetBlockSize()
    #    if blockxsize == ds.RasterXSize:
    #        ds.GetRasterBand(1).Checksum()
    ds = None

    # Release memory associated to the in-memory file
    #gdal.Unlink('/vsimem/test.tif')
