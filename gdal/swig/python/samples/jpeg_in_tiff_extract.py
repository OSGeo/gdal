#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR samples
# Purpose:  Extract a JPEG file from a JPEG-in-TIFF tile/strip
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

import sys

from osgeo import gdal


def Usage():
    print('Usage: jpeg_in_tiff_extract.py in.tif out.jpg [tile_x tile_y [band_nbr]]')
    print('')
    print('Extract a JPEG file from a JPEG-in-TIFF tile/strip.')
    print('If tile_x tile_y are not specified, then all tiles/strips are extracted')
    print('in filenames out_[bandnbr_]tx_ty.jpg')
    print('')
    return 1

###############################################################


def extract_tile(ds, src_band_nbr, tile_x, tile_y, jpg_filename):

    block_offset = ds.GetRasterBand(src_band_nbr).GetMetadataItem('BLOCK_OFFSET_%d_%d' % (tile_x, tile_y), 'TIFF')
    block_size = ds.GetRasterBand(src_band_nbr).GetMetadataItem('BLOCK_SIZE_%d_%d' % (tile_x, tile_y), 'TIFF')
    if block_offset is None or block_size is None:
        print('ERROR: Cannot find block (%d,%d)' % (tile_x, tile_y))
        return 1

    jpegtables = ds.GetRasterBand(src_band_nbr).GetMetadataItem('JPEGTABLES', 'TIFF')
    if jpegtables is not None:
        if (len(jpegtables) % 2) != 0 or jpegtables[0:4] != 'FFD8' or jpegtables[-2:] != 'D9':
            print('ERROR: Invalid JPEG tables')
            print(jpegtables)
            return 1

        # Remove final D9
        jpegtables = jpegtables[0:-2]

    tiff_f = gdal.VSIFOpenL(ds.GetDescription(), 'rb')
    if tiff_f is None:
        print('ERROR: Cannot reopen %s' % ds.GetDescription())
        return 1

    out_f = gdal.VSIFOpenL(jpg_filename, 'wb')
    if out_f is None:
        print('ERROR: Cannot create %s' % jpg_filename)
        gdal.VSIFCloseL(tiff_f)
        return 1

    # Write JPEG tables
    if jpegtables is not None:
        for i in range(int(len(jpegtables) / 2)):
            c1 = ord(jpegtables[2 * i])
            c2 = ord(jpegtables[2 * i + 1])
            if c1 >= ord('0') and c1 <= ord('9'):
                val = c1 - ord('0')
            else:
                val = (c1 - ord('A')) + 10
            val = val * 16
            if c2 >= ord('0') and c2 <= ord('9'):
                val = val + (c2 - ord('0'))
            else:
                val = val + (c2 - ord('A')) + 10
            gdal.VSIFWriteL(chr(val), 1, 1, out_f)
    else:
        gdal.VSIFWriteL(chr(0xFF), 1, 1, out_f)
        gdal.VSIFWriteL(chr(0xD8), 1, 1, out_f)

    # Write Adobe APP14 marker if necessary
    interleave = ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE')
    photometric = ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE')
    if interleave == 'PIXEL' and photometric == 'JPEG' and ds.RasterCount == 3:
        adobe_app14 = [0xFF, 0xEE, 0x00, 0x0E, 0x41, 0x64, 0x6F, 0x62, 0x65, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00]
        for c in adobe_app14:
            gdal.VSIFWriteL(chr(c), 1, 1, out_f)

    # Write JPEG codestream
    # skip leading 0xFF 0xD8
    gdal.VSIFSeekL(tiff_f, int(block_offset) + 2, 0)
    data = gdal.VSIFReadL(1, int(block_size) - 2, tiff_f)
    gdal.VSIFCloseL(tiff_f)
    gdal.VSIFWriteL(data, 1, len(data), out_f)

    gdal.VSIFCloseL(out_f)

    aux_xml_filename = '%s.aux.xml' % jpg_filename
    gt = ds.GetGeoTransform()
    srs = ds.GetProjectionRef()
    if srs is not None and srs != '':
        sub_gt = [gt[i] for i in range(6)]
        (blockxsize, blockysize) = ds.GetRasterBand(1).GetBlockSize()
        sub_gt[0] = gt[0] + tile_x * blockxsize * gt[1]
        sub_gt[3] = gt[3] + tile_y * blockysize * gt[5]

        out_f = gdal.VSIFOpenL(aux_xml_filename, 'wb')
        if out_f is None:
            print('ERROR: Cannot create %s' % aux_xml_filename)
            return 1
        content = """<PAMDataset>
    <SRS>%s</SRS>
    <GeoTransform>%.18g,%.18g,%.18g,%.18g,%.18g,%.18g</GeoTransform>
    </PAMDataset>
    """ % (srs, sub_gt[0], sub_gt[1], sub_gt[2], sub_gt[3], sub_gt[4], sub_gt[5])
        gdal.VSIFWriteL(content, 1, len(content), out_f)
        gdal.VSIFCloseL(out_f)
    else:
        gdal.Unlink('%s.aux.xml' % jpg_filename)

    return 0

###############################################################


def jpeg_in_tiff_extract(argv):

    if len(argv) < 2:
        print('ERROR: Not enough arguments')
        return Usage()

    tiff_filename = argv[0]
    jpg_filename = argv[1]
    if len(argv) >= 3:
        tile_x = int(argv[2])
        tile_y = int(argv[3])
        if len(argv) == 5:
            band_nbr = int(argv[4])
        else:
            band_nbr = None
    else:
        tile_x = None
        tile_y = None

    radix_jpg_filename = jpg_filename
    extensions = ['.jpg', '.jpeg', '.JPG', '.JPEG']
    extension = None
    for ext in extensions:
        pos = radix_jpg_filename.find(ext)
        if pos >= 0:
            extension = ext
            radix_jpg_filename = radix_jpg_filename[0:pos]
            break
    if pos < 0:
        print('ERROR: %s should end with .jpg/.jpeg' % jpg_filename)
        return 1

    ds = gdal.Open(tiff_filename)
    if ds is None:
        print('ERROR: Cannot open %s' % tiff_filename)
        return 1

    if ds.GetDriver() is None or \
       ds.GetDriver().GetDescription() != 'GTiff':
        print('ERROR: %s is not a TIFF dataset.' % tiff_filename)
        return 1

    photometric = ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE')
    interleave = ds.GetMetadataItem('INTERLEAVE', 'IMAGE_STRUCTURE')

    if photometric != 'JPEG' and photometric != 'YCbCr JPEG':
        print('ERROR: %s is not a JPEG-compressed TIFF dataset.' % tiff_filename)
        return 1

    (blockxsize, blockysize) = ds.GetRasterBand(1).GetBlockSize()
    if blockysize == 1:
        blockysize = ds.RasterYSize
    block_in_row = (ds.RasterXSize + blockxsize - 1) / blockxsize
    block_in_col = (ds.RasterYSize + blockysize - 1) / blockysize

    # Extract single tile ?
    if tile_x is not None:

        if tile_x < 0 or tile_x >= block_in_row:
            print('ERROR: Invalid tile_x : %d. Should be >= 0 and < %d' % (tile_x, block_in_row))
            return 1
        if tile_y < 0 or tile_y >= block_in_col:
            print('ERROR: Invalid tile_y : %d. Should be >= 0 and < %d' % (tile_y, block_in_col))
            return 1

        if ds.RasterCount > 1:
            if interleave == 'PIXEL':
                if band_nbr is not None:
                    print('ERROR: For a INTERLEAVE=PIXEL dataset, band_nbr should NOT be specified')
                    return 1
            else:
                if band_nbr is None:
                    print('ERROR: For a INTERLEAVE=BAND dataset, band_nbr should be specified')
                    return 1

        if band_nbr is not None:
            if band_nbr < 1 or band_nbr >= ds.RasterCount:
                print('ERROR: Invalid band_nbr : %d. Should be >= 1 and <= %d' % (tile_y, ds.RasterCount))
                return 1

        if band_nbr is not None:
            src_band_nbr = band_nbr
        else:
            src_band_nbr = 1

        return extract_tile(ds, src_band_nbr, tile_x, tile_y, jpg_filename)

    # Extract all tiles
    else:
        if ds.RasterCount == 1 or interleave == 'PIXEL':
            for tile_y in range(block_in_col):
                for tile_x in range(block_in_row):
                    filename = '%s_%d_%d%s' % (radix_jpg_filename, tile_x, tile_y, extension)
                    ret = extract_tile(ds, 1, tile_x, tile_y, filename)
                    if ret != 0:
                        return ret
        else:
            for src_band_nbr in range(ds.RasterCount):
                for tile_y in range(block_in_col):
                    for tile_x in range(block_in_row):
                        filename = '%s_%d_%d_%d%s' % (radix_jpg_filename, src_band_nbr + 1, tile_x, tile_y, extension)
                        ret = extract_tile(ds, src_band_nbr + 1, tile_x, tile_y, filename)
                        if ret != 0:
                            return ret
        return 0


def main(argv):
    gdal.GeneralCmdLineProcessor(argv)
    return jpeg_in_tiff_extract(argv[1:])


if __name__ == '__main__':
    sys.exit(main(sys.argv))

