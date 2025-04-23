#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VSI path specific options
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2022 Even Rouault <even dot rouault at spatialys dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def test_vsi_path_specific_options():

    with pytest.raises(Exception):
        gdal.GetPathSpecificOption(None, "key")

    with pytest.raises(Exception):
        gdal.GetPathSpecificOption("prefix", None)

    assert gdal.GetPathSpecificOption("prefix", "key") is None

    assert gdal.GetPathSpecificOption("prefix", "key", "default") == "default"

    with pytest.raises(Exception):
        gdal.SetPathSpecificOption(None, "key", "value")

    with pytest.raises(Exception):
        gdal.SetPathSpecificOption("prefix", None, "value")

    gdal.SetPathSpecificOption("prefix", "key", "value")
    assert gdal.GetPathSpecificOption("prefix", "key") == "value"
    assert gdal.GetPathSpecificOption("prefix/object", "key") == "value"
    assert gdal.GetPathSpecificOption("prefix", "key", "default") == "value"
    assert gdal.GetPathSpecificOption("another_prefix", "key") is None

    gdal.SetPathSpecificOption("prefix", "key", None)
    assert gdal.GetPathSpecificOption("prefix", "key") is None

    gdal.SetPathSpecificOption("prefix", "key", "value")
    gdal.ClearPathSpecificOptions("prefix")
    assert gdal.GetPathSpecificOption("prefix", "key") is None

    gdal.SetPathSpecificOption("prefix", "key", "value")
    gdal.ClearPathSpecificOptions("another_prefix")
    assert gdal.GetPathSpecificOption("prefix", "key") == "value"
    gdal.ClearPathSpecificOptions()
    assert gdal.GetPathSpecificOption("prefix", "key") is None
