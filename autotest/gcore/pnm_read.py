#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a PNM file.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

###############################################################################
# When imported build a list of units based on the files available.

init_list = [("byte.pnm", 4672), ("uint16.pnm", 4672)]


@pytest.mark.parametrize(
    "filename,checksum",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.require_driver("PNM")
def test_pnm_open(filename, checksum):
    ut = gdaltest.GDALTest("PNM", filename, 1, checksum)
    ut.testOpen()
