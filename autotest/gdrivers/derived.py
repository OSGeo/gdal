#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test derived driver
# Author:   Julien Michel <julien dot michel at cnes dot fr>
#
###############################################################################
# Copyright (c) 2016, Julien Michel, <julien dot michel at cnes dot fr>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("Derived")


def test_derived_test1():
    filename = "../gcore/data/cfloat64.tif"
    gdal.ErrorReset()
    ds = gdal.Open(filename)
    assert ds is not None and gdal.GetLastErrorMsg() == ""
    got_dsds = ds.GetMetadata("DERIVED_SUBDATASETS")
    expected_gt = ds.GetGeoTransform()
    expected_prj = ds.GetProjection()
    expected_dsds = {
        "DERIVED_SUBDATASET_1_NAME": "DERIVED_SUBDATASET:AMPLITUDE:../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_1_DESC": "Amplitude of input bands from ../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_2_NAME": "DERIVED_SUBDATASET:PHASE:../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_2_DESC": "Phase of input bands from ../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_3_NAME": "DERIVED_SUBDATASET:REAL:../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_3_DESC": "Real part of input bands from ../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_4_NAME": "DERIVED_SUBDATASET:IMAG:../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_4_DESC": "Imaginary part of input bands from ../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_5_NAME": "DERIVED_SUBDATASET:CONJ:../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_5_DESC": "Conjugate of input bands from ../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_6_NAME": "DERIVED_SUBDATASET:INTENSITY:../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_6_DESC": "Intensity (squared amplitude) of input bands from ../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_7_NAME": "DERIVED_SUBDATASET:LOGAMPLITUDE:../gcore/data/cfloat64.tif",
        "DERIVED_SUBDATASET_7_DESC": "log10 of amplitude of input bands from ../gcore/data/cfloat64.tif",
    }

    if got_dsds != expected_dsds:
        import pprint

        pprint.pprint(got_dsds)
        pytest.fail()

    for key in expected_dsds:
        val = expected_dsds[key]
        if key.endswith("_NAME"):
            ds = gdal.Open(val)
            assert ds is not None and gdal.GetLastErrorMsg() == ""
            gt = ds.GetGeoTransform()
            if gt != expected_gt:
                import pprint

                pprint.pprint(
                    "Expected geotransform: " + str(expected_gt) + ", got " + str(gt)
                )
                pytest.fail()
            prj = ds.GetProjection()
            if prj != expected_prj:
                import pprint

                pprint.pprint(
                    "Expected projection: " + str(expected_prj) + ", got: " + str(gt)
                )
                pytest.fail()


def test_derived_test2():
    filename = "../gcore/data/cint_sar.tif"
    gdal.ErrorReset()
    ds = gdal.Open(filename)
    assert ds is not None and gdal.GetLastErrorMsg() == ""
    got_dsds = ds.GetMetadata("DERIVED_SUBDATASETS")
    expected_dsds = {
        "DERIVED_SUBDATASET_1_NAME": "DERIVED_SUBDATASET:AMPLITUDE:../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_1_DESC": "Amplitude of input bands from ../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_2_NAME": "DERIVED_SUBDATASET:PHASE:../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_2_DESC": "Phase of input bands from ../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_3_NAME": "DERIVED_SUBDATASET:REAL:../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_3_DESC": "Real part of input bands from ../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_4_NAME": "DERIVED_SUBDATASET:IMAG:../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_4_DESC": "Imaginary part of input bands from ../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_5_NAME": "DERIVED_SUBDATASET:CONJ:../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_5_DESC": "Conjugate of input bands from ../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_6_NAME": "DERIVED_SUBDATASET:INTENSITY:../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_6_DESC": "Intensity (squared amplitude) of input bands from ../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_7_NAME": "DERIVED_SUBDATASET:LOGAMPLITUDE:../gcore/data/cint_sar.tif",
        "DERIVED_SUBDATASET_7_DESC": "log10 of amplitude of input bands from ../gcore/data/cint_sar.tif",
    }

    expected_cs = {
        "DERIVED_SUBDATASET_1_NAME": 345,
        "DERIVED_SUBDATASET_2_NAME": 10,
        "DERIVED_SUBDATASET_3_NAME": 159,
        "DERIVED_SUBDATASET_4_NAME": 142,
        "DERIVED_SUBDATASET_5_NAME": 110,
        "DERIVED_SUBDATASET_6_NAME": 314,
        "DERIVED_SUBDATASET_7_NAME": 55,
    }

    if got_dsds != expected_dsds:
        import pprint

        pprint.pprint(got_dsds)
        pytest.fail()

    for key in expected_dsds:
        val = expected_dsds[key]
        if key.endswith("_NAME"):
            ds = gdal.Open(val)
            assert ds is not None and gdal.GetLastErrorMsg() == ""
            cs = ds.GetRasterBand(1).Checksum()
            if expected_cs[key] != cs:
                import pprint

                pprint.pprint(
                    "Expected checksum " + str(expected_cs[key]) + ", got " + str(cs)
                )
                pytest.fail()


# Error cases


def test_derived_test3():

    with pytest.raises(Exception):
        gdal.Open("DERIVED_SUBDATASET:LOGAMPLITUDE")

    with pytest.raises(Exception):
        gdal.Open("DERIVED_SUBDATASET:invalid_alg:../gcore/data/byte.tif")

    with pytest.raises(Exception):
        gdal.Open("DERIVED_SUBDATASET:LOGAMPLITUDE:dataset_does_not_exist")

    with pytest.raises(Exception):
        # Raster with zero band
        gdal.Open("DERIVED_SUBDATASET:LOGAMPLITUDE:data/hdf5/CSK_DGM.h5")


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_derived_vrt_errors():
    for function in [
        "real",
        "imag",
        "complex",
        "mod",
        "phase",
        "conj",
        "sum",
        "diff",
        "mul",
        "cmul",
        "inv",
        "intensity",
        "sqrt",
        "log10",
        "dB",
        "dB2amp",
        "dB2pow",
    ]:
        ds = gdal.Open(
            '<VRTDataset rasterXSize="1" rasterYSize="1"><VRTRasterBand subClass="VRTDerivedRasterBand"><PixelFunctionType>%s</PixelFunctionType></VRTRasterBand></VRTDataset>'
            % function
        )
        with pytest.raises(Exception):
            ds.GetRasterBand(1).Checksum()


# test that metadata is transferred


def test_derived_test4():
    filename = "../gcore/data/byte_rpc.tif"
    gdal.ErrorReset()
    ds = gdal.Open(filename)
    assert ds is not None and gdal.GetLastErrorMsg() == ""
    derived_ds = gdal.Open("DERIVED_SUBDATASET:LOGAMPLITUDE:" + filename)
    assert "RPC" in derived_ds.GetMetadataDomainList()
    assert ds.GetMetadata("RPC") == derived_ds.GetMetadata("RPC")
