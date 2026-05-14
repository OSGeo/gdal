#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test CPHD driver support.
# Author:   Norman Barker <norman@analyticaspatial.com>
#
###############################################################################
# Copyright (c) 2026, Norman Barker <norman@analyticaspatial.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

from pathlib import Path

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("CPHD")

attribute_names = [
    "cphd_version",
    "classification",
    "collect_type",
    "collector_name",
    "core_name",
    "radar_mode",
    "xml",
]


def test_cphd_local():
    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    # test CPHD file generated with sarkit and then edited, it does not contain valid data
    filename = Path(__file__).parent / "data" / "cphd" / "test.cphd"
    with gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER) as ds:
        assert ds
        rg = ds.GetRootGroup()
        assert rg
        assert rg.GetGroupNames() == ["1"]
        attrs = rg.GetAttributes()
        assert attrs
        attr_names = [a.GetName() for a in attrs]
        assert sorted(list(set(attr_names) - set(attribute_names))) == [
            "k",
            "release_info",
        ]

        # test most used attributes
        assert rg.GetAttribute("cphd_version").Read() == "1.1.0"
        assert rg.GetAttribute("classification").Read() == "UNCLASSIFIED"
        assert rg.GetAttribute("collect_type").Read() == "MONOSTATIC"
        assert rg.GetAttribute("collector_name").Read() == "Synthetic"
        assert rg.GetAttribute("core_name").Read() == "SyntheticCore"
        assert rg.GetAttribute("radar_mode").Read() == "SPOTLIGHT"
        xml = rg.GetAttribute("xml").Read()
        assert xml.startswith("<CPHD ")

        # test custom attribute
        assert rg.GetAttribute("k").Read() == "V"

        assert rg.GetMDArrayNames() == []

        grp = rg.OpenGroup("1")
        assert grp

        # check array names
        array_names = grp.GetMDArrayNames()
        assert array_names
        assert array_names == ["SignalBlock", "PVP"]

        # check dimensions
        pvp_array = grp.OpenMDArray("PVP")
        assert pvp_array.GetDimensionCount() == 1
        assert pvp_array.GetDimensions()[0].GetName() == "Vector"
        assert pvp_array.GetDimensions()[0].GetSize() == 1

        # check compound data type
        cdt = pvp_array.GetDataType()
        assert cdt.GetName() == "PVPDataType"
        assert cdt.GetClass() == gdal.GEDTC_COMPOUND
        assert cdt.GetSize() == 360
        comps = cdt.GetComponents()
        assert len(comps) == 25

        # check first few components
        assert comps[0].GetName() == "TxTime"
        assert comps[0].GetOffset() == 0
        assert comps[0].GetType().GetNumericDataType() == gdal.GDT_Float64

        assert comps[1].GetName() == "TxPos"
        assert comps[1].GetOffset() == 8
        tx_dt = comps[1].GetType()
        assert tx_dt.GetClass() == gdal.GEDTC_COMPOUND
        assert len(tx_dt.GetComponents()) == 3
        assert tx_dt.GetComponents()[0].GetName() == "X"
        assert tx_dt.GetComponents()[1].GetName() == "Y"
        assert tx_dt.GetComponents()[2].GetName() == "Z"

        # read first few vector parameters, they are all zero
        arr = pvp_array.ReadAsArray(array_start_idx=[0], count=[1])
        assert arr["TxTime"][0] == 0
        assert arr["TxPos"][0]["X"] == 0
        assert arr["TxPos"][0]["Y"] == 0
        assert arr["TxPos"][0]["Z"] == 0

        # test TxAntenna.TxACX, TxAntenna.TxACY and TxAntenna.TxEB
        assert arr["TxAntenna.TxACX"][0]["X"] == 0
        assert arr["TxAntenna.TxACX"][0]["Y"] == 0
        assert arr["TxAntenna.TxACX"][0]["Z"] == 0
        assert arr["TxAntenna.TxACY"][0]["X"] == 0
        assert arr["TxAntenna.TxACY"][0]["Y"] == 0
        assert arr["TxAntenna.TxACY"][0]["Z"] == 0
        assert arr["TxAntenna.TxEB"][0]["DCX"] == 0
        assert arr["TxAntenna.TxEB"][0]["DCY"] == 0

        # test RcvAntenna.RcvACX, RcvAntenna.RcvACY and RcvAntenna.RcvEB
        assert arr["RcvAntenna.RcvACX"][0]["X"] == 0
        assert arr["RcvAntenna.RcvACX"][0]["Y"] == 0
        assert arr["RcvAntenna.RcvACX"][0]["Z"] == 0
        assert arr["RcvAntenna.RcvACY"][0]["X"] == 0
        assert arr["RcvAntenna.RcvACY"][0]["Y"] == 0
        assert arr["RcvAntenna.RcvACY"][0]["Z"] == 0
        assert arr["RcvAntenna.RcvEB"][0]["DCX"] == 0
        assert arr["RcvAntenna.RcvEB"][0]["DCY"] == 0

        # test AddedPVP MyPVP
        assert arr["MyPVP"][0] == 0

        # test signal block
        signal_array = grp.OpenMDArray("SignalBlock")
        assert signal_array.GetDimensionCount() == 2
        assert signal_array.GetDimensions()[0].GetName() == "Y"
        assert signal_array.GetDimensions()[1].GetName() == "X"
        assert signal_array.GetDimensions()[0].GetSize() == 1
        assert signal_array.GetDimensions()[1].GetSize() == 1

        arr = signal_array.ReadAsArray(
            array_start_idx=[0, 0],
            count=[1, 1],
        )
        assert arr[0][0] == np.complex64(0 + 0j)


@pytest.mark.require_curl()
@pytest.mark.network
@pytest.mark.parametrize(
    "file",
    [
        (
            "umbra-open-data-catalog",
            "sar-data/tasks/ad hoc/Angel of the North - Gateshead, UK/afe15b3b-f91d-4a0c-a65e-13802cda11c6/2024-01-14-09-53-26_UMBRA-06/2024-01-14-09-53-26_UMBRA-06_CPHD.cphd",
        ),
        (
            "capella-open-data",
            "data/2025/7/15/CAPELLA_C17_SP_CPHD_HH_20250715000343_20250715000414/CAPELLA_C17_SP_CPHD_HH_20250715000343_20250715000414.cphd",
        ),
    ],
)
def test_cphd_multidim_basic(file):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    bucket, obj = file
    options = {
        "AWS_S3_ENDPOINT": "https://s3.amazonaws.com",
        "AWS_NO_SIGN_REQUEST": "YES",
        "AWS_VIRTUAL_HOSTING": "TRUE",
        "AWS_REGION": "us-west-2",
    }

    with gdaltest.credentials("/vsis3/" + bucket, options):
        # check whether file is cached for local development
        # CPHD files are typically large and
        # are not included in the test suite
        filename = Path(__file__).parent / "data" / "cphd" / Path(obj).name
        if not filename.exists():
            filename = "/vsis3/" + bucket + "/" + obj

            if gdal.VSIStatL(filename) is None:
                pytest.skip(f"{filename} no longer existing or reachable")

        with gdal.OpenEx(filename, gdal.OF_MULTIDIM_RASTER) as ds:
            rg = ds.GetRootGroup()
            if bucket == "umbra-open-data-catalog":
                assert rg
                assert rg.GetGroupNames() == ["Primary"]
                attrs = rg.GetAttributes()
                assert attrs
                attr_names = [a.GetName() for a in attrs]
                assert attr_names == attribute_names

                # test most used attributes
                assert rg.GetAttribute("cphd_version").Read() == "1.1.0"
                assert rg.GetAttribute("classification").Read() == "UNCLASSIFIED"
                assert rg.GetAttribute("collect_type").Read() == "MONOSTATIC"
                assert rg.GetAttribute("collector_name").Read() == "Umbra-06"
                assert (
                    rg.GetAttribute("core_name").Read()
                    == "2024-01-14T09:53:27_Umbra-06"
                )
                assert rg.GetAttribute("radar_mode").Read() == "SPOTLIGHT"
                xml = rg.GetAttribute("xml").Read()
                assert xml.startswith("<CPHD ")

                assert rg.GetMDArrayNames() == ["Antenna", "Antenna_element"]

                grp = rg.OpenGroup("Primary")
                assert grp

                # check array names
                array_names = grp.GetMDArrayNames()
                assert array_names
                assert array_names == ["SignalBlock", "PVP"]

                # check dimensions
                pvp_array = grp.OpenMDArray("PVP")
                assert pvp_array.GetDimensionCount() == 1
                assert pvp_array.GetDimensions()[0].GetName() == "Vector"
                assert pvp_array.GetDimensions()[0].GetSize() == 28281

                # check compound data type
                cdt = pvp_array.GetDataType()
                assert cdt.GetName() == "PVPDataType"
                assert cdt.GetClass() == gdal.GEDTC_COMPOUND
                assert cdt.GetSize() == 240
                comps = cdt.GetComponents()
                assert len(comps) == 20

                # check first few components
                assert comps[0].GetName() == "TxTime"
                assert comps[0].GetOffset() == 0
                assert comps[0].GetType().GetNumericDataType() == gdal.GDT_Float64

                assert comps[1].GetName() == "TxPos"
                assert comps[1].GetOffset() == 8
                tx_dt = comps[1].GetType()
                assert tx_dt.GetClass() == gdal.GEDTC_COMPOUND
                assert len(tx_dt.GetComponents()) == 3
                assert tx_dt.GetComponents()[0].GetName() == "X"
                assert tx_dt.GetComponents()[1].GetName() == "Y"
                assert tx_dt.GetComponents()[2].GetName() == "Z"

                # read first few vector parameters, these values were extracted using sarkit
                # https://sarkit.readthedocs.io/en/stable/index.html
                arr = pvp_array.ReadAsArray(array_start_idx=[0], count=[1])
                assert arr["TxTime"][0] == pytest.approx(0.003194269)
                assert arr["TxPos"][0]["X"] == 4212215.919356111
                assert arr["TxPos"][0]["Y"] == -314182.3195983262
                assert arr["TxPos"][0]["Z"] == 5453564.126859501

                # test signal block
                signal_array = grp.OpenMDArray("SignalBlock")
                assert signal_array.GetDimensionCount() == 2
                assert signal_array.GetDimensions()[0].GetName() == "Y"
                assert signal_array.GetDimensions()[1].GetName() == "X"
                assert signal_array.GetDimensions()[0].GetSize() == 28281
                assert signal_array.GetDimensions()[1].GetSize() == 27647

                arr = signal_array.ReadAsArray(
                    array_start_idx=[0, 0],
                    count=[1, 1],
                )
                assert arr[0][0] == np.complex64(2.8269463 + -7.8994064j)
            else:
                # capella open-data
                assert rg
                assert rg.GetGroupNames() == ["0"]
                attrs = rg.GetAttributes()
                assert attrs
                attr_names = [a.GetName() for a in attrs]
                assert list(set(attr_names) - set(attribute_names)) == ["release_info"]

                # test most used attributes
                assert rg.GetAttribute("cphd_version").Read() == "1.1.0"
                assert rg.GetAttribute("classification").Read() == "UNCLASSIFIED"
                assert rg.GetAttribute("collect_type").Read() == "MONOSTATIC"
                assert rg.GetAttribute("collector_name").Read() == "capella-radar-17"
                assert (
                    rg.GetAttribute("core_name").Read()
                    == "15JUL25_CAPELLA-17_001_000343_SL0000R_51N012E_001X_HH_0101_SPY"
                )
                assert rg.GetAttribute("radar_mode").Read() == "SPOTLIGHT"
                xml = rg.GetAttribute("xml").Read()
                assert xml.startswith("<CPHD ")

                assert rg.GetMDArrayNames() == []

                grp = rg.OpenGroup("0")
                assert grp

                # check array names
                array_names = grp.GetMDArrayNames()
                assert array_names
                assert array_names == ["SignalBlock", "PVP"]

                # check dimensions
                pvp_array = grp.OpenMDArray("PVP")
                assert pvp_array.GetDimensionCount() == 1
                assert pvp_array.GetDimensions()[0].GetName() == "Vector"
                assert pvp_array.GetDimensions()[0].GetSize() == 318339

                # check compound data type
                cdt = pvp_array.GetDataType()
                assert cdt.GetName() == "PVPDataType"
                assert cdt.GetClass() == gdal.GEDTC_COMPOUND
                assert cdt.GetSize() == 264
                comps = cdt.GetComponents()
                assert len(comps) == 23

                # check first few components
                assert comps[0].GetName() == "TxTime"
                assert comps[0].GetOffset() == 0
                assert comps[0].GetType().GetNumericDataType() == gdal.GDT_Float64

                assert comps[1].GetName() == "TxPos"
                assert comps[1].GetOffset() == 8
                tx_dt = comps[1].GetType()
                assert tx_dt.GetClass() == gdal.GEDTC_COMPOUND
                assert len(tx_dt.GetComponents()) == 3
                assert tx_dt.GetComponents()[0].GetName() == "X"
                assert tx_dt.GetComponents()[1].GetName() == "Y"
                assert tx_dt.GetComponents()[2].GetName() == "Z"

                # read first few vector parameters, these values were extracted using sarkit
                # https://sarkit.readthedocs.io/en/stable/index.html
                arr = pvp_array.ReadAsArray(array_start_idx=[0], count=[1])
                assert arr["TxTime"][0] == pytest.approx(-6.56320714e-09)
                assert arr["TxPos"][0]["X"] == 4478974.61632389
                assert arr["TxPos"][0]["Y"] == 533801.6269820664
                assert arr["TxPos"][0]["Z"] == 5303717.530705924

                # Capella uses amplitude scaling
                assert arr["AmpSF"][0] == pytest.approx(0.00148429, abs=1e-8)

                # test signal block
                signal_array = grp.OpenMDArray("SignalBlock")
                assert signal_array.GetDimensionCount() == 2
                assert signal_array.GetDimensions()[0].GetName() == "Y"
                assert signal_array.GetDimensions()[1].GetName() == "X"
                assert signal_array.GetDimensions()[0].GetSize() == 318339
                assert signal_array.GetDimensions()[1].GetSize() == 28802

                arr = signal_array.ReadAsArray(
                    array_start_idx=[0, 0],
                    count=[1, 1],
                )
                assert arr[0][0] == np.complex64(-46 + 24j)
