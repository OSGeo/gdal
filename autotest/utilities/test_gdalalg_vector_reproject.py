#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector reproject' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr


def get_reproject_alg():
    return gdal.GetGlobalAlgorithmRegistry()["vector"]["reproject"]


# Most of the testing is done in test_gdalalg_vector_pipeline.py


@pytest.mark.require_driver("OSM")
def test_gdalalg_vector_reproject_dataset_getnextfeature():

    alg = get_reproject_alg()
    src_ds = gdal.OpenEx("../ogr/data/osm/test.pbf")
    alg["input"] = src_ds
    alg["dst-crs"] = "EPSG:4326"

    assert alg.ParseCommandLineArguments(
        ["--of", "stream", "--output", "streamed_output"]
    )
    assert alg.Run()

    out_ds = alg["output"].GetDataset()
    assert out_ds.TestCapability(ogr.ODsCRandomLayerRead)

    expected = []
    while True:
        f, _ = src_ds.GetNextFeature()
        if not f:
            break
        expected.append(str(f))

    got = []
    out_ds.ResetReading()
    while True:
        f, _ = out_ds.GetNextFeature()
        if not f:
            break
        got.append(str(f))

    assert expected == got
