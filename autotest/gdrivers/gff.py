#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GFF driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

###############################################################################
# Test an extract from a real dataset


def test_gff_1():
    # 12088 = 2048 + 8 * 1255
    gdaltest.download_or_skip(
        "http://sandia.gov/RADAR/complex_data/MiniSAR20050519p0001image008.gff",
        "MiniSAR20050519p0001image008.gff",
        12088,
    )

    tst = gdaltest.GDALTest(
        "GFF",
        "tmp/cache/MiniSAR20050519p0001image008.gff",
        1,
        -1,
        filename_absolute=1,
    )
    with pytest.raises(Exception):
        tst.testOpen()
