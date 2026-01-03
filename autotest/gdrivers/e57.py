#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functionality for E57 driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal

pytestmark = [
    pytest.mark.require_driver("E57"),
    pytest.mark.require_driver("JPEG"),
    pytest.mark.require_driver("PNG"),
]


def test_e57_no_image():

    with pytest.raises(Exception):
        gdal.Open("data/e57/empty.e57")


def test_e57_single_image():

    with pytest.raises(Exception):
        gdal.Open("data/e57/fake.e57", gdal.GA_Update)

    ds = gdal.Open("data/e57/fake.e57")
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetMetadata_Dict() == {
        "ASSOCIATED_DATA_3D_GUID": "associatedData3DGuid",
        "NAME": "image name",
        "POSE_ROTATION_W": "rotation.w",
        "POSE_ROTATION_Z": "rotation.z",
        "POSE_TRANSLATION_X": "translation.x",
        "POSE_TRANSLATION_Y": "translation.y",
        "POSE_TRANSLATION_Z": "translation.z",
        "REPRESENTATION_TYPE": "spherical",
    }
    assert ds.GetMetadataItem("REPRESENTATION_TYPE") == "spherical"
    assert ds.GetMetadata_Dict("IMAGE_STRUCTURE") == {"JPEG_QUALITY": "75"}
    assert ds.GetRasterBand(1).Checksum() != 0
    assert ds.GetMetadataDomainList() == [
        "IMAGE_STRUCTURE",
        "DERIVED_SUBDATASETS",
        "xml:E57",
    ]
    assert ds.GetMetadata("xml:E57")[0].startswith('<e57Root type="Structure"')

    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 2435


def test_e57_two_images():
    ds = gdal.Open("data/e57/fake_two_images.e57")
    assert ds.RasterXSize == 0
    assert ds.RasterYSize == 0
    assert ds.RasterCount == 0
    assert ds.GetMetadataDomainList() == ["SUBDATASETS", "xml:E57"]
    assert ds.GetDriver().ShortName == "E57"

    assert ds.GetMetadata("SUBDATASETS") == {
        "SUBDATASET_1_NAME": 'E57:"data/e57/fake_two_images.e57":image',
        "SUBDATASET_1_DESC": "Image image (0x0)",
        "SUBDATASET_2_NAME": 'E57:"data/e57/fake_two_images.e57":image2',
        "SUBDATASET_2_DESC": "Image image2 (0x0)",
    }

    with pytest.raises(Exception):
        gdal.Open("E57:foo")

    with pytest.raises(Exception):
        gdal.Open("E57:data/e57/fake_two_images.e57")

    with pytest.raises(Exception):
        gdal.Open("E57:/i/do_not/exist:unexisting")

    with pytest.raises(Exception):
        gdal.Open("E57:data/e57/fake_two_images.e57:unexisting")

    ds = gdal.Open('E57:"data/e57/fake_two_images.e57":image')
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() != 0
    assert ds.GetMetadata("xml:E57")[0].startswith('<e57Root type="Structure"')
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_ALL_VALID
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4873

    ds = gdal.Open('E57:"data/e57/fake_two_images.e57":image2')
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetMetadata("xml:E57")[0].startswith('<e57Root type="Structure"')


def test_e57_pam(tmp_vsimem):

    filename = tmp_vsimem / "test.e57"
    with gdal.VSIFile("data/e57/fake_two_images.e57", "rb") as f_in:
        with gdal.VSIFile(filename, "wb") as f_out:
            f_out.write(f_in.read())

    with gdal.Open(f'E57:"{filename}":image') as ds:
        ds.GetRasterBand(1).ComputeStatistics(False)

    with gdal.Open(f'E57:"{filename}":image') as ds:
        assert ds.GetRasterBand(1).GetStatistics(False, False)[1] == 255


def test_e57_errors(tmp_vsimem):

    filename = tmp_vsimem / "test.e57"
    with gdal.VSIFile("data/e57/fake.e57", "rb") as f:
        data = bytearray(f.read())

    for i in range(1024):
        original_byte_value = data[i]
        data[i] = 255 - data[i]
        gdal.FileFromMemBuffer(filename, data)
        try:
            with gdal.quiet_errors():
                gdal.Open(filename)
        except Exception:
            pass
        data[i] = original_byte_value


def test_e57_overviews(tmp_vsimem):

    filename = tmp_vsimem / "test.e57"
    with gdal.VSIFile("data/e57/fake_two_images.e57", "rb") as f_in:
        with gdal.VSIFile(filename, "wb") as f_out:
            f_out.write(f_in.read())

    with gdal.Open(f'E57:"{filename}":image') as ds:
        ds.BuildOverviews("NEAR", [2])

    assert gdal.VSIStatL(str(filename) + "_0.ovr")

    with gdal.Open(f'E57:"{filename}":image') as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 1

    with gdal.Open(f'E57:"{filename}":image2') as ds:
        ds.BuildOverviews("NEAR", [2, 4])

    assert gdal.VSIStatL(str(filename) + "_1.ovr")

    with gdal.Open(f'E57:"{filename}":image2') as ds:
        assert ds.GetRasterBand(1).GetOverviewCount() == 2
