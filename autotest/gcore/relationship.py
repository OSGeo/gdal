#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDALRelationship
# Author:   Nyall Dawson, <nyall dot dawson at gmail dot com>
#
###############################################################################
# Copyright (c) 2022, Nyall Dawson <nyall dot dawson at gmail dot com>
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

import pytest

from osgeo import gdal


def test_gdal_relationship():

    with pytest.raises(ValueError):
        gdal.Relationship(None, None, None, gdal.GRC_ONE_TO_ONE)

    relationship = gdal.Relationship(
        "name", "left_table", "right_table", gdal.GRC_MANY_TO_ONE
    )
    assert relationship.GetName() == "name"
    assert relationship.GetCardinality() == gdal.GRC_MANY_TO_ONE
    assert relationship.GetLeftTableName() == "left_table"
    assert relationship.GetRightTableName() == "right_table"

    relationship = gdal.Relationship(
        "name", "left_table", "right_table", gdal.GRC_ONE_TO_ONE
    )
    assert relationship.GetCardinality() == gdal.GRC_ONE_TO_ONE

    assert relationship.GetMappingTableName() == ""
    relationship.SetMappingTableName("mapping_table")
    assert relationship.GetMappingTableName() == "mapping_table"

    assert relationship.GetLeftTableFields() is None
    relationship.SetLeftTableFields(["a_field"])
    assert relationship.GetLeftTableFields() == ["a_field"]

    assert relationship.GetRightTableFields() is None
    relationship.SetRightTableFields(["b_field", "c_field"])
    assert relationship.GetRightTableFields() == ["b_field", "c_field"]

    assert relationship.GetLeftMappingTableFields() is None
    relationship.SetLeftMappingTableFields(["a_field2"])
    assert relationship.GetLeftMappingTableFields() == ["a_field2"]

    assert relationship.GetRightMappingTableFields() is None
    relationship.SetRightMappingTableFields(["b_field2", "c_field2"])
    assert relationship.GetRightMappingTableFields() == ["b_field2", "c_field2"]

    assert relationship.GetType() == gdal.GRT_ASSOCIATION
    relationship.SetType(gdal.GRT_AGGREGATION)
    assert relationship.GetType() == gdal.GRT_AGGREGATION

    assert relationship.GetForwardPathLabel() == ""
    relationship.SetForwardPathLabel("forward path")
    assert relationship.GetForwardPathLabel() == "forward path"

    assert relationship.GetBackwardPathLabel() == ""
    relationship.SetBackwardPathLabel("backward path")
    assert relationship.GetBackwardPathLabel() == "backward path"

    assert relationship.GetRelatedTableType() == ""
    relationship.SetRelatedTableType("media")
    assert relationship.GetRelatedTableType() == "media"
