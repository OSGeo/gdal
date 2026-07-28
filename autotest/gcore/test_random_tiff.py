#!/usr/bin/env python
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test "random" TIFF files
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import random

from osgeo import gdal

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

tags = [
    ["TIFFTAG_IMAGEWIDTH", TIFFTAG_IMAGEWIDTH, [0, 1, 0x7FFFFFFF, 0xFFFFFFFF]],
    ["TIFFTAG_IMAGELENGTH", TIFFTAG_IMAGELENGTH, [0, 1, 0x7FFFFFFF, 0xFFFFFFFF]],
    ["TIFFTAG_BITSPERSAMPLE", TIFFTAG_BITSPERSAMPLE, [None, 1, 8, 255]],
    # ['TIFFTAG_COMPRESSION', TIFFTAG_COMPRESSION, [None, COMPRESSION_NONE]],
    # ['TIFFTAG_PHOTOMETRIC', TIFFTAG_PHOTOMETRIC, [None, PHOTOMETRIC_MINISBLACK]],
    ["TIFFTAG_STRIPOFFSETS", TIFFTAG_STRIPOFFSETS, [None, 0, 8]],
    ["TIFFTAG_SAMPLESPERPIXEL", TIFFTAG_SAMPLESPERPIXEL, [None, 1, 255, 65535]],
    [
        "TIFFTAG_ROWSPERSTRIP",
        TIFFTAG_ROWSPERSTRIP,
        [None, 0, 1, 0x7FFFFFFF, 0xFFFFFFFF],
    ],
    [
        "TIFFTAG_STRIPBYTECOUNTS",
        TIFFTAG_STRIPBYTECOUNTS,
        [None, 0, 1, 0x7FFFFFFF, 0xFFFFFFFF],
    ],
    [
        "TIFFTAG_PLANARCONFIG",
        TIFFTAG_PLANARCONFIG,
        [None, PLANARCONFIG_CONTIG, PLANARCONFIG_SEPARATE],
    ],
    ["TIFFTAG_TILEWIDTH", TIFFTAG_TILEWIDTH, [None, 0, 8, 256, 65536, 0x7FFFFFFF]],
    ["TIFFTAG_TILELENGTH", TIFFTAG_TILELENGTH, [None, 0, 8, 256, 65536, 0x7FFFFFFF]],
    ["TIFFTAG_TILEOFFSETS", TIFFTAG_TILEOFFSETS, [None, 0, 8]],
    [
        "TIFFTAG_TILEBYTECOUNTS",
        TIFFTAG_TILEBYTECOUNTS,
        [None, 0, 1, 0x7FFFFFFF, 0xFFFFFFFF],
    ],
    # ['TIFFTAG_SAMPLEFORMAT', TIFFTAG_SAMPLEFORMAT, [None, SAMPLEFORMAT_UINT]],
]


def generate_tif(comb_val):
    idx_tab = []
    count_non_none = 0
    has_strip = False
    for tag in tags:
        code = tag[1]
        possible_vals = tag[2]
        len_possible_vals = tag[3]
        idx_val = comb_val % len_possible_vals
        comb_val = int(comb_val / len_possible_vals)
        val = possible_vals[idx_val]

        if (
            code == TIFFTAG_TILEWIDTH
            or code == TIFFTAG_TILELENGTH
            or code == TIFFTAG_TILEOFFSETS
            or code == TIFFTAG_TILEBYTECOUNTS
        ):
            if has_strip:
                idx_val = 0
                val = None
        idx_tab.append(idx_val)

        if val is not None:
            if (
                code == TIFFTAG_STRIPOFFSETS
                or code == TIFFTAG_ROWSPERSTRIP
                or code == TIFFTAG_STRIPBYTECOUNTS
            ):
                has_strip = True
            count_non_none = count_non_none + 1

    content = "\x49\x49\x2a\x00\x08\x00\x00\x00" + ("%c" % count_non_none) + "\x00"
    for level, tag in enumerate(tags):
        code = tag[1]
        possible_vals = tag[2]
        idx = idx_tab[level]
        val = possible_vals[idx]
        if val is not None:
            content = content + ("%c" % (code & 255)) + ("%c" % (code >> 8))
            content = content + "\x04\x00\x01\x00\x00\x00"
            content = (
                content
                + ("%c" % (val & 255))
                + ("%c" % ((val >> 8) & 255))
                + ("%c" % ((val >> 16) & 255))
                + ("%c" % ((val >> 24) & 255))
            )

    return content


def run_test():
    nVals = 1
    for level, tag in enumerate(tags):
        len_possible_vals = len(tag[2])
        tags[level].append(len_possible_vals)
        nVals = nVals * len_possible_vals
    itern = 0
    while True:
        itern += 1
        comb_val = random.randrange(0, nVals)
        content = generate_tif(comb_val)

        # Create in-memory file
        gdal.FileFromMemBuffer("/vsimem/test.tif", content)

        print("iter = %d, comb_val_init = %d" % (itern, comb_val))
        gdal.Open("/vsimem/test.tif")


if __name__ == "__main__":
    run_test()
