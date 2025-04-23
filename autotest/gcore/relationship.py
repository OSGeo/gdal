#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDALRelationship
# Author:   Nyall Dawson, <nyall dot dawson at gmail dot com>
#
###############################################################################
# Copyright (c) 2022, Nyall Dawson <nyall dot dawson at gmail dot com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


def test_gdal_relationship():

    with pytest.raises(Exception):
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
