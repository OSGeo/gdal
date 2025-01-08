#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  LOS/LAS Testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os

import gdaltest
import pytest

pytestmark = pytest.mark.require_driver("LOSLAS")


###############################################################################


def test_loslas_1():

    tst = gdaltest.GDALTest(
        "LOSLAS", "data/loslas/wyhpgn.los", 1, 0, filename_absolute=1
    )
    gt = (-111.625, 0.25, 0.0, 45.625, 0.0, -0.25)
    stats = (
        -0.027868999168276787,
        0.033906999975442886,
        0.009716129862575248,
        0.008260044951413324,
    )
    tst.testOpen(check_gt=gt, check_stat=stats, check_prj="WGS84")
    os.unlink("data/loslas/wyhpgn.los.aux.xml")
