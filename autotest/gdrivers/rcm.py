#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test RCM driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("RCM")


def test_rcm_open_from_root_dir():
    ds = gdal.Open("data/rcm/fake_VV_VH_GRD")
    assert ds.GetDriver().ShortName == "RCM"
    assert ds.RasterCount == 2


def test_rcm_open_from_metadata_dir():
    ds = gdal.Open("data/rcm/fake_VV_VH_GRD/metadata")
    assert ds.GetDriver().ShortName == "RCM"
    assert ds.RasterCount == 2


def test_rcm_open_from_product_xml():
    ds = gdal.Open("data/rcm/fake_VV_VH_GRD/metadata/product.xml")
    assert ds.GetDriver().ShortName == "RCM"
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 17915
    assert ds.RasterYSize == 3297
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).Checksum() == 0
    assert ds.GetRasterBand(1).GetMetadata() == {"POLARIMETRIC_INTERP": "VH"}
    got_md = ds.GetMetadata()
    new_got_md = {}
    for k in got_md:
        new_got_md[k] = got_md[k].replace("\\", "/")
    assert new_got_md == {
        "ACQUISITION_START_TIME": "rawDataStartTime",
        "ACQUISITION_TYPE": "Medium Resolution 50m",
        "BEAMS": "beams",
        "BEAM_MODE": "Medium Resolution 50m",
        "BEAM_MODE_DEFINITION_ID": "beamModeDefinitionId",
        "BEAM_MODE_MNEMONIC": "beamModeMnemonic",
        "BETA_NOUGHT_LUT": "data/rcm/fake_VV_VH_GRD/metadata/calibration/lutBeta_VH.xml,data/rcm/fake_VV_VH_GRD/metadata/calibration/lutBeta_VV.xml",
        "BITS_PER_SAMPLE": "16",
        "DATA_TYPE": "Integer",
        "FACILITY_IDENTIFIER": "inputDatasetFacilityId",
        "FAR_RANGE_INCIDENCE_ANGLE": "incAngFarRng",
        "FIRST_LINE_TIME": "zeroDopplerTimeFirstLine",
        "GAMMA_LUT": "data/rcm/fake_VV_VH_GRD/metadata/calibration/lutGamma_VH.xml,data/rcm/fake_VV_VH_GRD/metadata/calibration/lutGamma_VV.xml",
        "GEODETIC_TERRAIN_HEIGHT": "200",
        "LAST_LINE_TIME": "zeroDopplerTimeLastLine",
        "LINE_SPACING": "sampledLineSpacing",
        "LINE_TIME_ORDERING": "Increasing",
        "LUT_APPLIED": "Mixed",
        "NEAR_RANGE_INCIDENCE_ANGLE": "incAngNearRng",
        "ORBIT_DATA_FILE": "orbitDataFileName",
        "ORBIT_DATA_SOURCE": "Downlinked",
        "ORBIT_DIRECTION": "Descending",
        "PER_POLARIZATION_SCALING": "true",
        "PIXEL_SPACING": "sampledPixelSpacing",
        "PIXEL_TIME_ORDERING": "Decreasing",
        "POLARIZATIONS": "VH VV",
        "POLARIZATION_DATA_MODE": "Dual Co/Cross",
        "PROCESSING_FACILITY": "processingFacility",
        "PROCESSING_TIME": "processingTime",
        "PRODUCT_ID": "productId",
        "PRODUCT_TYPE": "GRD",
        "SAMPLED_LINE_SPACING_TIME": "sampledLineSpacingTime",
        "SAMPLED_PIXEL_SPACING_TIME": "sampledPixelSpacingTime",
        "SAMPLE_TYPE": "Magnitude Detected",
        "SATELLITE_HEIGHT": "600000",
        "SATELLITE_IDENTIFIER": "RCM-1",
        "SECURITY_CLASSIFICATION": "Non classifié / Unclassified",
        "SENSOR_IDENTIFIER": "SAR",
        "SIGMA_NOUGHT_LUT": "data/rcm/fake_VV_VH_GRD/metadata/calibration/lutSigma_VH.xml,data/rcm/fake_VV_VH_GRD/metadata/calibration/lutSigma_VV.xml",
        "SLANT_RANGE_FAR_EDGE": "slantRangeFarEdge",
        "SLANT_RANGE_NEAR_EDGE": "slantRangeNearEdge",
    }
    assert ds.GetMetadata("RPC") == {
        "ERR_BIAS": "0",
        "ERR_RAND": "0",
        "HEIGHT_OFF": "0",
        "HEIGHT_SCALE": "0",
        "LAT_OFF": "0",
        "LAT_SCALE": "0",
        "LINE_DEN_COEFF": "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
        "LINE_NUM_COEFF": "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
        "LINE_OFF": "0",
        "LINE_SCALE": "0",
        "LONG_OFF": "0",
        "LONG_SCALE": "0",
        "SAMP_DEN_COEFF": "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
        "SAMP_NUM_COEFF": "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
        "SAMP_OFF": "0",
        "SAMP_SCALE": "0",
    }
    assert ds.GetGCPSpatialRef().GetAuthorityCode(None) == "4326"
    assert ds.GetGCPCount() == 1
    assert ds.GetGCPs()[0].GCPPixel == 2
    assert ds.GetGCPs()[0].GCPLine == 1
    assert ds.GetGCPs()[0].GCPX == 2.5
    assert ds.GetGCPs()[0].GCPY == 1.5
    assert ds.GetGCPs()[0].GCPZ == 3.5


def test_rcm_open_subdatasets():

    gdal.Open("RCM_CALIB:BETA0:data/rcm/fake_VV_VH_GRD/metadata/product.xml")
    gdal.Open("RCM_CALIB:SIGMA0:data/rcm/fake_VV_VH_GRD/metadata/product.xml")
    gdal.Open("RCM_CALIB:GAMMA:data/rcm/fake_VV_VH_GRD/metadata/product.xml")
    gdal.Open("RCM_CALIB:UNCALIB:data/rcm/fake_VV_VH_GRD/metadata/product.xml")
    with pytest.raises(Exception, match="Unsupported calibration type"):
        gdal.Open("RCM_CALIB:unhandled:data/rcm/fake_VV_VH_GRD/metadata/product.xml")
    with pytest.raises(Exception):
        gdal.Open("RCM_CALIB:UNCALIB:i_do_not_exist/product.xml")


@pytest.mark.require_curl
def test_rcm_open_real_dataset():
    remote_file = "https://donnees-data.asc-csa.gc.ca/users/OpenData_DonneesOuvertes/pub/RCM/Antarctica/RCM3_OK2120467_PK2120468_3_SC30MCPB_20200124_083635_CH_CV_MLC/metadata/product.xml"

    if gdaltest.gdalurlopen(remote_file) is None:
        pytest.skip(f"Could not read from {remote_file}")

    ds = gdal.Open("/vsicurl/" + remote_file)
    assert ds.GetDriver().ShortName == "RCM"
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
