#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal mdim reproject' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


@pytest.mark.require_driver("netCDF")
def test_gdalalg_mdim_pipeline_stream():

    with gdal.alg.mdim.pipeline(
        pipeline="read ../gdrivers/data/netcdf/byte.nc ! write --output=streamed --output-format stream"
    ) as alg:
        ds = alg.Output()
        assert ds.GetDriver().GetDescription() == "netCDF"
        assert ds.GetRootGroup().OpenMDArray("Band1")


@pytest.mark.require_driver("netCDF")
def test_gdalalg_mdim_pipeline_write(tmp_path):

    with gdal.alg.mdim.pipeline(
        pipeline=f"read ../gdrivers/data/netcdf/byte.nc ! write {tmp_path}/out.nc"
    ) as alg:
        ds = alg.Output()
        assert ds.GetDriver().GetDescription() == "netCDF"
        assert ds.GetRootGroup().OpenMDArray("Band1")


def test_gdalalg_mdim_pipeline_help_doc():

    import gdaltest
    import test_cli_utilities

    gdal_path = test_cli_utilities.get_gdal_path()
    if gdal_path is None:
        pytest.skip("gdal binary missing")

    out = gdaltest.runexternal(f"{gdal_path} mdim pipeline --help-doc=main")

    assert "Usage: gdal mdim pipeline [OPTIONS] <PIPELINE>" in out
    assert "<PIPELINE> is of the form: " in out

    out = gdaltest.runexternal(f"{gdal_path} mdim pipeline --help-doc=info")

    assert "* info [OPTIONS]" in out

    out, _ = gdaltest.runexternal_out_and_err(
        f"{gdal_path} mdim pipeline --help-doc=unknown"
    )

    assert "ERROR: unknown pipeline step 'unknown'" in out
