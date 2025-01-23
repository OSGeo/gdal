#!/usr/bin/env pytest
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VRTProcessedDataset support.
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import math
import os

import gdaltest
import pytest

from osgeo import gdal

from .vrtderived import _validate

pytestmark = pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)

np = pytest.importorskip("numpy")
gdal_array = pytest.importorskip("osgeo.gdal_array")

###############################################################################
# Test error cases in general VRTProcessedDataset XML structure


def test_vrtprocesseddataset_errors(tmp_vsimem):

    with pytest.raises(Exception, match="Input element missing"):
        gdal.Open(
            """<VRTDataset subclass='VRTProcessedDataset'>
                    </VRTDataset>
                    """
        )

    with pytest.raises(
        Exception,
        match="Input element should have a SourceFilename or VRTDataset element",
    ):
        gdal.Open(
            """<VRTDataset subclass='VRTProcessedDataset'>
                    <Input/>
                    </VRTDataset>
                    """
        )

    with pytest.raises(Exception):  # "No such file or directory'", but O/S dependent
        gdal.Open(
            """<VRTDataset subclass='VRTProcessedDataset'>
                    <Input><SourceFilename/></Input>
                    </VRTDataset>
                    """
        )

    with pytest.raises(
        Exception,
        match="Missing one of rasterXSize, rasterYSize or bands on VRTDataset",
    ):
        gdal.Open(
            """<VRTDataset subclass='VRTProcessedDataset'>
                    <Input><VRTDataset/></Input>
                    </VRTDataset>
                    """
        )

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 10, 5, 3)
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(2).Fill(2)
    src_ds.GetRasterBand(3).Fill(3)
    src_ds.Close()

    with pytest.raises(Exception, match="Invalid value of 'unscale'"):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input unscale="maybe">
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        </VRTDataset>
            """
        )

    with pytest.raises(Exception, match="ProcessingSteps element missing"):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        </VRTDataset>
            """
        )

    with pytest.raises(
        Exception, match="Inconsistent declared VRT dimensions with input dataset"
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset' rasterXSize='1'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        </VRTDataset>
            """
        )

    with pytest.raises(
        Exception, match="Inconsistent declared VRT dimensions with input dataset"
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset' rasterYSize='1'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        </VRTDataset>
            """
        )

    with pytest.raises(Exception, match="At least one step should be defined"):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps/>
        </VRTDataset>
            """
        )


###############################################################################
# Test nominal cases of BandAffineCombination algorithm


@pytest.mark.parametrize("INTERLEAVE", ["PIXEL", "BAND"])
def test_vrtprocesseddataset_affine_combination_nominal(tmp_vsimem, INTERLEAVE):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(
        src_filename, 2, 1, 3, options=["INTERLEAVE=" + INTERLEAVE]
    )
    src_ds.GetRasterBand(1).WriteArray(np.array([[1, 3]]))
    src_ds.GetRasterBand(2).WriteArray(np.array([[2, 6]]))
    src_ds.GetRasterBand(3).WriteArray(np.array([[3, 3]]))
    src_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step name="Affine combination of band values">
            <Algorithm>BandAffineCombination</Algorithm>
            <Argument name="coefficients_1">10,0,1,0</Argument>
            <Argument name="coefficients_2">20,0,0,1</Argument>
            <Argument name="coefficients_3">30,1,0,0</Argument>
            <Argument name="min">15</Argument>
            <Argument name="max">32</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    assert ds.RasterXSize == 2
    assert ds.RasterYSize == 1
    assert ds.RasterCount == 3
    assert ds.GetSpatialRef() is None
    assert ds.GetGeoTransform(can_return_null=True) is None
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    np.testing.assert_equal(ds.GetRasterBand(1).ReadAsArray(), np.array([[15, 10 + 6]]))
    np.testing.assert_equal(
        ds.GetRasterBand(2).ReadAsArray(), np.array([[20 + 3, 20 + 3]])
    )
    np.testing.assert_equal(ds.GetRasterBand(3).ReadAsArray(), np.array([[30 + 1, 32]]))


###############################################################################
# Test several steps in a VRTProcessedDataset


def test_vrtprocesseddataset_several_steps(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 10, 5, 3)
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(2).Fill(2)
    src_ds.GetRasterBand(3).Fill(3)
    src_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step>
            <Algorithm>BandAffineCombination</Algorithm>
            <Argument name="coefficients_1">0,0,1,0</Argument>
            <Argument name="coefficients_2">0,0,0,1</Argument>
            <Argument name="coefficients_3">0,1,0,0</Argument>
        </Step>
        <Step>
            <Algorithm>BandAffineCombination</Algorithm>
            <Argument name="coefficients_1">0,0,1,0</Argument>
            <Argument name="coefficients_2">0,0,0,1</Argument>
            <Argument name="coefficients_3">0,1,0,0</Argument>
        </Step>
        <Step>
            <Algorithm>BandAffineCombination</Algorithm>
            <Argument name="coefficients_1">0,0,1,0</Argument>
            <Argument name="coefficients_2">0,0,0,1</Argument>
            <Argument name="coefficients_3">0,1,0,0</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    assert ds.RasterXSize == 10
    assert ds.RasterYSize == 5
    assert ds.RasterCount == 3
    assert ds.GetSpatialRef() is None
    assert ds.GetGeoTransform(can_return_null=True) is None
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    assert ds.GetRasterBand(1).ComputeRasterMinMax(False) == (1, 1)
    assert ds.GetRasterBand(2).ComputeRasterMinMax(False) == (2, 2)
    assert ds.GetRasterBand(3).ComputeRasterMinMax(False) == (3, 3)


###############################################################################
# Test nominal cases of BandAffineCombination algorithm with nodata


def test_vrtprocesseddataset_affine_combination_nodata(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 1, 2)
    src_ds.GetRasterBand(1).WriteArray(np.array([[1, 2]]))
    src_ds.GetRasterBand(1).SetNoDataValue(1)
    src_ds.GetRasterBand(2).WriteArray(np.array([[3, 3]]))
    src_ds.GetRasterBand(2).SetNoDataValue(1)
    src_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step name="Affine combination of band values">
            <Algorithm>BandAffineCombination</Algorithm>
            <Argument name="coefficients_1">0,1,1</Argument>
            <Argument name="coefficients_2">0,1,-1</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    np.testing.assert_equal(ds.GetRasterBand(1).ReadAsArray(), np.array([[1, 5]]))
    # 0 should actually be 3-2=1, but this is the nodata value hence the replacement value
    np.testing.assert_equal(ds.GetRasterBand(2).ReadAsArray(), np.array([[1, 0]]))


def test_vrtprocesseddataset_affine_combination_nodata_as_parameter(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 1, 2)
    src_ds.GetRasterBand(1).WriteArray(np.array([[1, 2]]))
    src_ds.GetRasterBand(2).WriteArray(np.array([[3, 3]]))
    src_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step name="Affine combination of band values">
            <Algorithm>BandAffineCombination</Algorithm>
            <Argument name="coefficients_1">0,1,1</Argument>
            <Argument name="coefficients_2">256,1,-1</Argument>
            <Argument name="src_nodata">1</Argument>
            <Argument name="dst_nodata">255</Argument>
            <Argument name="dst_intended_datatype">Byte</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    np.testing.assert_equal(ds.GetRasterBand(1).ReadAsArray(), np.array([[255, 5]]))
    # 254 should actually be 256+1*2+(-1)*3=255, but this is the nodata value hence the replacement value
    np.testing.assert_equal(ds.GetRasterBand(2).ReadAsArray(), np.array([[255, 254]]))


###############################################################################
# Test replacement_nodata logic of BandAffineCombination


def test_vrtprocesseddataset_affine_combination_replacement_nodata(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 1, 2)
    src_ds.GetRasterBand(1).WriteArray(np.array([[1, 2]]))
    src_ds.GetRasterBand(2).WriteArray(np.array([[3, 3]]))
    src_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step name="Affine combination of band values">
            <Algorithm>BandAffineCombination</Algorithm>
            <Argument name="coefficients_1">0,1,1</Argument>
            <Argument name="coefficients_2">256,1,-1</Argument>
            <Argument name="src_nodata">1</Argument>
            <Argument name="dst_nodata">255</Argument>
            <Argument name="replacement_nodata">128</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    np.testing.assert_equal(ds.GetRasterBand(1).ReadAsArray(), np.array([[255, 5]]))
    # 254 should actually be 256+1*2+(-1)*3=255, but this is the nodata value hence the replacement value
    np.testing.assert_equal(ds.GetRasterBand(2).ReadAsArray(), np.array([[255, 128]]))


###############################################################################
# Test error cases of BandAffineCombination algorithm


def test_vrtprocesseddataset_affine_combination_errors(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 10, 5, 3)
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(2).Fill(2)
    src_ds.GetRasterBand(3).Fill(3)
    src_ds.Close()

    with pytest.raises(
        Exception,
        match="Step 'Affine combination of band values' lacks required Argument 'coefficients_{band}'",
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step name="Affine combination of band values">
                <Algorithm>BandAffineCombination</Algorithm>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(
        Exception, match="Argument coefficients_1 has 3 values, whereas 4 are expected"
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step name="Affine combination of band values">
                <Algorithm>BandAffineCombination</Algorithm>
                <Argument name="coefficients_1">10,0,1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(Exception, match="Argument coefficients_3 is missing"):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step name="Affine combination of band values">
                <Algorithm>BandAffineCombination</Algorithm>
                <Argument name="coefficients_1">10,0,1,0</Argument>
                <Argument name="coefficients_2">10,0,1,0</Argument>
                <Argument name="coefficients_4">10,0,1,0</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(
        Exception,
        match="Final step expect 3 bands, but only 1 coefficient_XX are provided",
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step name="Affine combination of band values">
                <Algorithm>BandAffineCombination</Algorithm>
                <Argument name="coefficients_1">10,0,1,0</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )


###############################################################################
# Test nominal cases of LUT algorithm


def test_vrtprocesseddataset_lut_nominal(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 3, 1, 2)
    src_ds.GetRasterBand(1).WriteArray(np.array([[1, 2, 3]]))
    src_ds.GetRasterBand(2).WriteArray(np.array([[1, 2, 3]]))
    src_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step>
            <Algorithm>LUT</Algorithm>
            <Argument name="lut_1">1.5:10,2.5:20</Argument>
            <Argument name="lut_2">1.5:100,2.5:200</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    np.testing.assert_equal(ds.GetRasterBand(1).ReadAsArray(), np.array([[10, 15, 20]]))
    np.testing.assert_equal(
        ds.GetRasterBand(2).ReadAsArray(), np.array([[100, 150, 200]])
    )


###############################################################################
# Test nominal cases of LUT algorithm with nodata coming from input dataset


def test_vrtprocesseddataset_lut_nodata(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 4, 1, 2)
    src_ds.GetRasterBand(1).WriteArray(np.array([[0, 1, 2, 3]]))
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    src_ds.GetRasterBand(2).WriteArray(np.array([[0, 1, 2, 3]]))
    src_ds.GetRasterBand(2).SetNoDataValue(0)
    src_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step>
            <Algorithm>LUT</Algorithm>
            <Argument name="lut_1">1.5:10,2.5:20</Argument>
            <Argument name="lut_2">1.5:100,2.5:200</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    np.testing.assert_equal(
        ds.GetRasterBand(1).ReadAsArray(), np.array([[0, 10, 15, 20]])
    )
    np.testing.assert_equal(
        ds.GetRasterBand(2).ReadAsArray(), np.array([[0, 100, 150, 200]])
    )


###############################################################################
# Test nominal cases of LUT algorithm with nodata set as a parameter


def test_vrtprocesseddataset_lut_nodata_as_parameter(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 4, 1, 2)
    src_ds.GetRasterBand(1).WriteArray(np.array([[0, 1, 2, 3]]))
    src_ds.GetRasterBand(2).WriteArray(np.array([[0, 1, 2, 3]]))
    src_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step>
            <Algorithm>LUT</Algorithm>
            <Argument name="lut_1">1.5:10,2.5:20</Argument>
            <Argument name="lut_2">1.5:100,2.5:200</Argument>
            <Argument name="src_nodata">0</Argument>
            <Argument name="dst_nodata">1</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    np.testing.assert_equal(
        ds.GetRasterBand(1).ReadAsArray(), np.array([[1, 10, 15, 20]])
    )
    np.testing.assert_equal(
        ds.GetRasterBand(2).ReadAsArray(), np.array([[1, 100, 150, 200]])
    )


###############################################################################
# Test error cases of LUT algorithm


def test_vrtprocesseddataset_lut_errors(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 3, 1, 2)
    src_ds.GetRasterBand(1).WriteArray(np.array([[1, 2, 3]]))
    src_ds.GetRasterBand(2).WriteArray(np.array([[1, 2, 3]]))
    src_ds.Close()

    with pytest.raises(Exception, match="Step 'nr 1' lacks required Argument"):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LUT</Algorithm>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(Exception, match="Invalid value for argument 'lut_1'"):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LUT</Algorithm>
                <Argument name="lut_1">1.5:10,2.5</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(Exception, match="Invalid band in argument 'lut_3'"):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LUT</Algorithm>
                <Argument name="lut_1">1.5:10,2.5:20</Argument>
                <Argument name="lut_3">1.5:10,2.5:20</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(Exception, match="Missing lut_XX element"):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LUT</Algorithm>
                <Argument name="lut_1">1.5:10,2.5:20</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )


###############################################################################
# Test nominal case of LocalScaleOffset algorithm


def test_vrtprocesseddataset_dehazing_nominal(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 6, 1, 2)
    src_ds.GetRasterBand(1).WriteArray(np.array([[1, 2, 3, 255, 1, 1]]))
    src_ds.GetRasterBand(2).WriteArray(np.array([[1, 2, 3, 255, 1, 1]]))
    src_ds.GetRasterBand(1).SetNoDataValue(255)
    src_ds.GetRasterBand(2).SetNoDataValue(255)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    src_ds.Close()

    gain_filename = str(tmp_vsimem / "gain.tif")
    gain_ds = gdal.GetDriverByName("GTiff").Create(gain_filename, 6, 1, 2)
    gain_ds.GetRasterBand(1).WriteArray(np.array([[2, 4, 6, 1, 254, 1]]))
    gain_ds.GetRasterBand(2).WriteArray(np.array([[3, 5, 7, 1, 254, 1]]))
    gain_ds.GetRasterBand(1).SetNoDataValue(254)
    gain_ds.GetRasterBand(2).SetNoDataValue(254)
    gain_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    gain_ds.Close()

    offset_filename = str(tmp_vsimem / "offset.tif")
    offset_ds = gdal.GetDriverByName("GTiff").Create(offset_filename, 6, 1, 2)
    offset_ds.GetRasterBand(1).WriteArray(np.array([[1, 2, 3, 1, 1, 253]]))
    offset_ds.GetRasterBand(2).WriteArray(np.array([[2, 3, 4, 1, 1, 253]]))
    offset_ds.GetRasterBand(1).SetNoDataValue(253)
    offset_ds.GetRasterBand(2).SetNoDataValue(253)
    offset_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    offset_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step>
            <Algorithm>LocalScaleOffset</Algorithm>
            <Argument name="gain_dataset_filename_1">{gain_filename}</Argument>
            <Argument name="gain_dataset_band_1">1</Argument>
            <Argument name="gain_dataset_filename_2">{gain_filename}</Argument>
            <Argument name="gain_dataset_band_2">2</Argument>
            <Argument name="offset_dataset_filename_1">{offset_filename}</Argument>
            <Argument name="offset_dataset_band_1">1</Argument>
            <Argument name="offset_dataset_filename_2">{offset_filename}</Argument>
            <Argument name="offset_dataset_band_2">2</Argument>
            <Argument name="min">2</Argument>
            <Argument name="max">16</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    np.testing.assert_equal(
        ds.GetRasterBand(1).ReadAsArray(), np.array([[2, 6, 15, 255, 255, 255]])
    )
    np.testing.assert_equal(
        ds.GetRasterBand(2).ReadAsArray(), np.array([[2, 7, 16, 255, 255, 255]])
    )


###############################################################################
# Test nominal case of LocalScaleOffset algorithm where gain and offset have a lower
# resolution than the input dataset


def test_vrtprocesseddataset_dehazing_different_resolution(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 6, 2, 1)
    src_ds.GetRasterBand(1).WriteArray(
        np.array([[1, 1, 2, 2, 3, 3], [1, 1, 2, 2, 3, 3]])
    )
    src_ds.SetGeoTransform([0, 0.5 * 10, 0, 0, 0, 0.5 * 10])
    src_ds.BuildOverviews("NEAR", [2])
    src_ds.Close()

    gain_filename = str(tmp_vsimem / "gain.tif")
    gain_ds = gdal.GetDriverByName("GTiff").Create(gain_filename, 3, 1, 1)
    gain_ds.GetRasterBand(1).WriteArray(np.array([[2, 4, 6]]))
    gain_ds.SetGeoTransform([0, 1 * 10, 0, 0, 0, 1 * 10])
    gain_ds.Close()

    offset_filename = str(tmp_vsimem / "offset.tif")
    offset_ds = gdal.GetDriverByName("GTiff").Create(offset_filename, 3, 1, 1)
    offset_ds.GetRasterBand(1).WriteArray(np.array([[1, 2, 3]]))
    offset_ds.SetGeoTransform([0, 1 * 10, 0, 0, 0, 1 * 10])
    offset_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step>
            <Algorithm>LocalScaleOffset</Algorithm>
            <Argument name="gain_dataset_filename_1">{gain_filename}</Argument>
            <Argument name="gain_dataset_band_1">1</Argument>
            <Argument name="offset_dataset_filename_1">{offset_filename}</Argument>
            <Argument name="offset_dataset_band_1">1</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    np.testing.assert_equal(
        ds.GetRasterBand(1).ReadAsArray(),
        np.array([[1, 2, 6, 8, 15, 15], [1, 2, 6, 8, 15, 15]]),
    )
    np.testing.assert_equal(
        ds.GetRasterBand(1).GetOverview(0).ReadAsArray(),
        np.array([[1, 6, 15]]),
    )


###############################################################################
# Test we properly request auxiliary datasets on the right-most/bottom-most
# truncated tile


def test_vrtprocesseddataset_dehazing_edge_effects(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(
        src_filename,
        257,
        257,
        1,
        gdal.GDT_Byte,
        ["TILED=YES", "BLOCKXSIZE=256", "BLOCKYSIZE=256"],
    )
    src_ds.GetRasterBand(1).Fill(10)
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    src_ds.Close()

    gain_filename = str(tmp_vsimem / "gain.tif")
    gain_ds = gdal.GetDriverByName("GTiff").Create(gain_filename, 1, 1)
    gain_ds.GetRasterBand(1).Fill(2)
    gain_ds.SetGeoTransform([0, 257, 0, 0, 0, -257])
    gain_ds.Close()

    offset_filename = str(tmp_vsimem / "offset.tif")
    offset_ds = gdal.GetDriverByName("GTiff").Create(offset_filename, 1, 1)
    offset_ds.GetRasterBand(1).Fill(3)
    offset_ds.SetGeoTransform([0, 257, 0, 0, 0, -257])
    offset_ds.Close()

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step>
            <Algorithm>LocalScaleOffset</Algorithm>
            <Argument name="gain_dataset_filename_1">{gain_filename}</Argument>
            <Argument name="gain_dataset_band_1">1</Argument>
            <Argument name="offset_dataset_filename_1">{offset_filename}</Argument>
            <Argument name="offset_dataset_band_1">1</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (17, 17)


###############################################################################
# Test error cases of LocalScaleOffset algorithm


def test_vrtprocesseddataset_dehazing_error(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 3, 1, 1)
    src_ds.GetRasterBand(1).WriteArray(np.array([[1, 2, 3]]))
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    src_ds.Close()

    with pytest.raises(
        Exception,
        match="Step 'nr 1' lacks required Argument 'offset_dataset_band_{band}'",
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LocalScaleOffset</Algorithm>
                <Argument name="gain_dataset_filename_1">{src_filename}</Argument>
                <Argument name="gain_dataset_band_1">1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(
        Exception,
        match="Invalid band in argument 'gain_dataset_filename_2'",
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LocalScaleOffset</Algorithm>
                <Argument name="gain_dataset_filename_2">{src_filename}</Argument>
                <Argument name="gain_dataset_band_1">1</Argument>
                <Argument name="offset_dataset_filename_1">{src_filename}</Argument>
                <Argument name="offset_dataset_band_1">1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(
        Exception,
        match="Invalid band in argument 'gain_dataset_band_2'",
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LocalScaleOffset</Algorithm>
                <Argument name="gain_dataset_filename_1">{src_filename}</Argument>
                <Argument name="gain_dataset_band_2">1</Argument>
                <Argument name="offset_dataset_filename_1">{src_filename}</Argument>
                <Argument name="offset_dataset_band_1">1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(
        Exception,
        match="Invalid band in argument 'offset_dataset_filename_2'",
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LocalScaleOffset</Algorithm>
                <Argument name="gain_dataset_filename_1">{src_filename}</Argument>
                <Argument name="gain_dataset_band_1">1</Argument>
                <Argument name="offset_dataset_filename_2">{src_filename}</Argument>
                <Argument name="offset_dataset_band_1">1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(
        Exception,
        match="Invalid band in argument 'offset_dataset_band_2'",
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LocalScaleOffset</Algorithm>
                <Argument name="gain_dataset_filename_1">{src_filename}</Argument>
                <Argument name="gain_dataset_band_1">1</Argument>
                <Argument name="offset_dataset_filename_1">{src_filename}</Argument>
                <Argument name="offset_dataset_band_2">1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(
        Exception,
        match=r"Invalid band number \(2\) for a gain dataset",
    ):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LocalScaleOffset</Algorithm>
                <Argument name="gain_dataset_filename_1">{src_filename}</Argument>
                <Argument name="gain_dataset_band_1">2</Argument>
                <Argument name="offset_dataset_filename_1">{src_filename}</Argument>
                <Argument name="offset_dataset_band_1">1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    with pytest.raises(Exception):  # "No such file or directory'", but O/S dependent
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LocalScaleOffset</Algorithm>
                <Argument name="gain_dataset_filename_1">invalid</Argument>
                <Argument name="gain_dataset_band_1">1</Argument>
                <Argument name="offset_dataset_filename_1">{src_filename}</Argument>
                <Argument name="offset_dataset_band_1">1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    nogt_filename = str(tmp_vsimem / "nogt.tif")
    ds = gdal.GetDriverByName("GTiff").Create(nogt_filename, 1, 1, 1)
    ds.Close()

    with pytest.raises(Exception, match="lacks a geotransform"):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>LocalScaleOffset</Algorithm>
                <Argument name="gain_dataset_filename_1">{nogt_filename}</Argument>
                <Argument name="gain_dataset_band_1">1</Argument>
                <Argument name="offset_dataset_filename_1">{nogt_filename}</Argument>
                <Argument name="offset_dataset_band_1">1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )


###############################################################################
# Test nominal cases of Trimming algorithm


def test_vrtprocesseddataset_trimming_nominal(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 6, 1, 4)

    R = 100.0
    G = 150.0
    B = 200.0
    NIR = 100.0

    src_ds.GetRasterBand(1).WriteArray(np.array([[int(R), 150, 200, 0, 0, 0]]))
    src_ds.GetRasterBand(2).WriteArray(np.array([[int(G), 200, 100, 0, 0, 0]]))
    src_ds.GetRasterBand(3).WriteArray(np.array([[int(B), 100, 150, 0, 0, 0]]))
    src_ds.GetRasterBand(4).WriteArray(np.array([[int(NIR), 150, 200, 0, 0, 0]]))
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    src_ds.Close()

    trimming_filename = str(tmp_vsimem / "trimming.tif")
    trimming_ds = gdal.GetDriverByName("GTiff").Create(trimming_filename, 6, 1, 1)

    localMaxRGB = 205.0

    trimming_ds.GetRasterBand(1).WriteArray(
        np.array([[int(localMaxRGB), 210, 220, 0, 0, 0]])
    )
    trimming_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    trimming_ds.Close()

    top_rgb = 200.0
    tone_ceil = 190.0
    top_margin = 0.1

    ds = gdal.Open(
        f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step>
            <Algorithm>Trimming</Algorithm>
            <Argument name="trimming_dataset_filename">{trimming_filename}</Argument>
            <Argument name="top_rgb">{top_rgb}</Argument>
            <Argument name="tone_ceil">{tone_ceil}</Argument>
            <Argument name="top_margin">{top_margin}</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    )

    # Do algorithm at hand

    # Extract local saturation value from trimming image
    reducedRGB = min((1.0 - top_margin) * top_rgb / localMaxRGB, 1)

    # RGB bands specific process
    maxRGB = max(R, G, B)
    toneMaxRGB = min(tone_ceil / maxRGB, 1)
    toneR = min(tone_ceil / R, 1)
    toneG = min(tone_ceil / G, 1)
    toneB = min(tone_ceil / B, 1)
    outputR = min(reducedRGB * R * toneR / toneMaxRGB, top_rgb)
    outputG = min(reducedRGB * G * toneG / toneMaxRGB, top_rgb)
    outputB = min(reducedRGB * B * toneB / toneMaxRGB, top_rgb)

    # Other bands processing (NIR, ...): only apply RGB reduction factor
    outputNIR = reducedRGB * NIR

    # print(outputR, outputG, outputB, outputNIR)

    assert round(outputR) == ds.GetRasterBand(1).ReadAsArray(0, 0, 1, 1)[0][0]
    assert round(outputG) == ds.GetRasterBand(2).ReadAsArray(0, 0, 1, 1)[0][0]
    assert round(outputB) == ds.GetRasterBand(3).ReadAsArray(0, 0, 1, 1)[0][0]
    assert round(outputNIR) == ds.GetRasterBand(4).ReadAsArray(0, 0, 1, 1)[0][0]

    np.testing.assert_equal(
        ds.GetRasterBand(1).ReadAsArray(),
        np.array([[92, 135, 164, 0, 0, 0]]),  # round(outputR)
    )
    np.testing.assert_equal(
        ds.GetRasterBand(2).ReadAsArray(),
        np.array([[139, 171, 86, 0, 0, 0]]),  # round(outputG)
    )
    np.testing.assert_equal(
        ds.GetRasterBand(3).ReadAsArray(),
        np.array([[176, 90, 129, 0, 0, 0]]),  # round(outputB)
    )
    np.testing.assert_equal(
        ds.GetRasterBand(4).ReadAsArray(),
        np.array([[88, 129, 164, 0, 0, 0]]),  # round(outputNIR)
    )


###############################################################################
# Test error cases of Trimming algorithm


def test_vrtprocesseddataset_trimming_errors(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 6, 1, 4)
    src_ds.GetRasterBand(1).WriteArray(np.array([[100, 150, 200, 0, 0, 0]]))
    src_ds.GetRasterBand(2).WriteArray(np.array([[150, 200, 100, 0, 0, 0]]))
    src_ds.GetRasterBand(3).WriteArray(np.array([[200, 100, 150, 0, 0, 0]]))
    src_ds.GetRasterBand(4).WriteArray(np.array([[100, 150, 200, 0, 0, 0]]))
    src_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    src_ds.Close()

    trimming_filename = str(tmp_vsimem / "trimming.tif")
    trimming_ds = gdal.GetDriverByName("GTiff").Create(trimming_filename, 6, 1, 1)
    trimming_ds.GetRasterBand(1).WriteArray(np.array([[200, 210, 220, 0, 0, 0]]))
    trimming_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    trimming_ds.Close()

    trimming_two_bands_filename = str(tmp_vsimem / "trimming_two_bands.tif")
    trimming_ds = gdal.GetDriverByName("GTiff").Create(
        trimming_two_bands_filename, 6, 1, 2
    )
    trimming_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    trimming_ds.Close()

    with pytest.raises(Exception):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>Trimming</Algorithm>
                <Argument name="trimming_dataset_filename">invalid</Argument>
                <Argument name="top_rgb">200</Argument>
                <Argument name="tone_ceil">190</Argument>
                <Argument name="top_margin">0.1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )

    for val in (0, 5):
        with pytest.raises(Exception, match="Invalid band in argument 'red_band'"):
            gdal.Open(
                f"""<VRTDataset subclass='VRTProcessedDataset'>
            <Input>
                <SourceFilename>{src_filename}</SourceFilename>
            </Input>
            <ProcessingSteps>
                <Step>
                    <Algorithm>Trimming</Algorithm>
                    <Argument name="trimming_dataset_filename">{trimming_filename}</Argument>
                    <Argument name="red_band">{val}</Argument>
                    <Argument name="top_rgb">200</Argument>
                    <Argument name="tone_ceil">190</Argument>
                    <Argument name="top_margin">0.1</Argument>
                </Step>
            </ProcessingSteps>
            </VRTDataset>
                """
            )

    for val in (0, 5):
        with pytest.raises(Exception, match="Invalid band in argument 'green_band'"):
            gdal.Open(
                f"""<VRTDataset subclass='VRTProcessedDataset'>
            <Input>
                <SourceFilename>{src_filename}</SourceFilename>
            </Input>
            <ProcessingSteps>
                <Step>
                    <Algorithm>Trimming</Algorithm>
                    <Argument name="trimming_dataset_filename">{trimming_filename}</Argument>
                    <Argument name="green_band">{val}</Argument>
                    <Argument name="top_rgb">200</Argument>
                    <Argument name="tone_ceil">190</Argument>
                    <Argument name="top_margin">0.1</Argument>
                </Step>
            </ProcessingSteps>
            </VRTDataset>
                """
            )

    for val in (0, 5):
        with pytest.raises(Exception, match="Invalid band in argument 'blue_band'"):
            gdal.Open(
                f"""<VRTDataset subclass='VRTProcessedDataset'>
            <Input>
                <SourceFilename>{src_filename}</SourceFilename>
            </Input>
            <ProcessingSteps>
                <Step>
                    <Algorithm>Trimming</Algorithm>
                    <Argument name="trimming_dataset_filename">{trimming_filename}</Argument>
                    <Argument name="blue_band">{val}</Argument>
                    <Argument name="top_rgb">200</Argument>
                    <Argument name="tone_ceil">190</Argument>
                    <Argument name="top_margin">0.1</Argument>
                </Step>
            </ProcessingSteps>
            </VRTDataset>
                """
            )

    for (red_band, green_band, blue_band) in [(1, 1, 3), (3, 2, 3), (1, 3, 3)]:
        with pytest.raises(
            Exception,
            match="red_band, green_band and blue_band must have distinct values",
        ):
            gdal.Open(
                f"""<VRTDataset subclass='VRTProcessedDataset'>
            <Input>
                <SourceFilename>{src_filename}</SourceFilename>
            </Input>
            <ProcessingSteps>
                <Step>
                    <Algorithm>Trimming</Algorithm>
                    <Argument name="trimming_dataset_filename">{trimming_filename}</Argument>
                    <Argument name="red_band">{red_band}</Argument>
                    <Argument name="green_band">{green_band}</Argument>
                    <Argument name="blue_band">{blue_band}</Argument>
                    <Argument name="top_rgb">200</Argument>
                    <Argument name="tone_ceil">190</Argument>
                    <Argument name="top_margin">0.1</Argument>
                </Step>
            </ProcessingSteps>
            </VRTDataset>
                """
            )

    with pytest.raises(Exception, match="Trimming dataset should have a single band"):
        gdal.Open(
            f"""<VRTDataset subclass='VRTProcessedDataset'>
        <Input>
            <SourceFilename>{src_filename}</SourceFilename>
        </Input>
        <ProcessingSteps>
            <Step>
                <Algorithm>Trimming</Algorithm>
                <Argument name="trimming_dataset_filename">{trimming_two_bands_filename}</Argument>
                <Argument name="top_rgb">200</Argument>
                <Argument name="tone_ceil">190</Argument>
                <Argument name="top_margin">0.1</Argument>
            </Step>
        </ProcessingSteps>
        </VRTDataset>
            """
        )


###############################################################################
# Test expressions


@pytest.mark.parametrize(
    "dialect,expression,src,expected,error,env",
    [
        pytest.param(
            "exprtk",
            "return [BANDS[1], BANDS[2]]",
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[[3, 4]], [[5, 6]]]),
            None,
            {},
            id="multiple bands in, multiple bands out (1)",
        ),
        pytest.param(
            "exprtk",
            "return [BANDS]",
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            None,
            {},
            id="multiple bands in, multiple bands out (2)",
        ),
        pytest.param(
            "muparser",
            "B2, B3",
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[[3, 4]], [[5, 6]]]),
            None,
            {},
            id="multiple bands in, multiple bands out (3)",
        ),
        pytest.param(
            "muparser",
            "BANDS",
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            None,
            {},
            id="multiple bands in, multiple bands out (4)",
        ),
        pytest.param(
            "exprtk",
            """
             // Reduce every 10 bands of input into a single
             // band of output, using avg()
             const var chunksize := 10;
             const var outsize := BANDS[] / chunksize;

             var chunk[chunksize];
             var out[outsize];

             for (var i := 0; i < out[]; i += 1) {
                for (var j := 0; j < chunk[]; j += 1) {
                    chunk[j] := BANDS[i * chunk[] + j];
                };
                out[i] := avg(chunk);
             };
             return [out];
             """,
            np.arange(100).reshape(50, 1, 2),
            np.array([[[9, 10]], [[29, 30]], [[49, 50]], [[69, 70]], [[89, 90]]]),
            None,
            {},
            id="procedural",
        ),
        pytest.param(
            "exprtk",
            "B1",
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[1, 2]]),
            None,
            {},
            id="multiple bands in, single band out (1)",
        ),
        pytest.param(
            "muparser",
            "B1",
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[1, 2]]),
            None,
            {},
            id="multiple bands in, single band out (2)",
        ),
        pytest.param(
            "exprtk",
            "BANDS[0]",
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[1, 2]]),
            None,
            {},
            id="multiple bands in, single band out (3)",
        ),
        pytest.param(
            "exprtk",
            "return [B1];",
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[1, 2]]),
            None,
            {},
            id="multiple bands in, single band out (4)",
        ),
        pytest.param(
            "exprtk",
            "return [B1, B2]",
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[1, 2]]),
            "returned 2 values but 1 output band",
            {},
            id="return wrong number of bands",
        ),
        pytest.param(
            "exprtk",
            "return [BANDS, B2]",
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[1, 2]]),
            "must return a vector or a list of scalars",
            {},
            id="return wrong number of bands",
        ),
        pytest.param(
            "exprtk",
            """
             var out[3];
             for (var i := 0; i < 100; i += 1) {
                out[i] := i;
             };
             return [out];
             """,
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[[1, 1]], [[2, 2]], [[3, 3]]]),
            "Attempted to access index 3",
            {},
            id="out of bounds vector access",
        ),
        pytest.param(
            "exprtk",
            """
             var out[5];
             return [out];
             """,
            np.array([[[1, 2]]]),
            np.array([[[1, 1]]]),
            "Failed to parse expression",
            {"GDAL_EXPRTK_MAX_VECTOR_LENGTH": "4"},
            id="vector too large",
        ),
        pytest.param(
            "exprtk",
            """
             var out[3];
             for (var i := 0; i < out[]; i += 1) {
                out[i] := i;
             };
             return [out];
             """,
            np.array([[[1, 2]], [[3, 4]], [[5, 6]]]),
            np.array([[[1, 1]], [[2, 2]], [[3, 3]]]),
            "Failed to parse expression",
            {"GDAL_EXPRTK_ENABLE_LOOPS": "NO"},
            id="loops disabled",
        ),
        pytest.param(
            "exprtk",
            """
             for (var i := 0; i < 20000; i += 1) {
                sleep(0.2/20000); // we only check runtime every 10,000 iterations
             };
             return [B1];
             """,
            np.array([[[1, 2]]]),
            np.array([[1, 2]]),
            "time exceeded maximum",
            {"GDAL_EXPRTK_TIMEOUT_SECONDS": "0.1"},
            id="loop evaluation timeout",
        ),
    ],
)
def test_vrtprocesseddataset_expression(
    request, tmp_vsimem, expression, src, dialect, expected, env, error
):
    if not gdaltest.gdal_has_vrt_expression_dialect(dialect):
        pytest.skip(f"{dialect} not available")

    if "timeout" in request.node.name and "debug" not in gdal.VersionInfo(""):
        pytest.skip("Timeout tests only work on debug builds")

    src_filename = tmp_vsimem / "src.tif"

    num_input_bands = 1 if len(src.shape) == 2 else src.shape[0]
    expected_output_bands = 1 if len(expected.shape) == 2 else expected.shape[0]

    with gdal.GetDriverByName("GTiff").Create(
        src_filename, 2, 1, num_input_bands
    ) as src_ds:
        src_ds.WriteArray(src)
        src_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])

    output_band_xml = "".join(
        f"""<VRTRasterBand band="{i + 1}" dataType="Float32" subClass="VRTProcessedRasterBand"/>"""
        for i in range(expected_output_bands)
    )

    vrt_xml = f"""<VRTDataset subclass='VRTProcessedDataset'>
            <Input>
                <SourceFilename>{src_filename}</SourceFilename>
            </Input>
            <ProcessingSteps>
                <Step>
                    <Algorithm>Expression</Algorithm>
                    <Argument name="expression">{expression.replace('<', '&lt;').replace('>', '&gt;')}</Argument>
                    <Argument name="dialect">{dialect}</Argument>
                </Step>
            </ProcessingSteps>
                {output_band_xml}
            </VRTDataset>
                """

    with gdal.config_options(env):
        if error:
            with pytest.raises(Exception, match=error):
                ds = gdal.Open(vrt_xml)
                result = ds.ReadAsArray()
        else:
            ds = gdal.Open(vrt_xml)
            result = ds.ReadAsArray()
            np.testing.assert_equal(result, expected)


@pytest.mark.parametrize(
    "batch_size",
    (
        pytest.param(13, id="batch size greater than input"),
        pytest.param(1, id="batch size=1"),
        pytest.param(3, id="input bands are multiple of batch size"),
        pytest.param(5, id="input bands are not a multiple of batch size"),
    ),
)
def test_vrtprocesseddataset_expression_batchsize(tmp_vsimem, batch_size):

    if not gdaltest.gdal_has_vrt_expression_dialect("muparser"):
        pytest.skip("muparser not available")

    src_filename = tmp_vsimem / "in.tif"

    inputs = np.arange(12)
    inputs = inputs.reshape(inputs.size, 1, 1)

    with gdal.GetDriverByName("GTiff").Create(
        src_filename,
        1,
        1,
        bands=inputs.size,
    ) as src_ds:
        src_ds.WriteArray(inputs)
        src_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])

    num_chunks = math.ceil(inputs.size / batch_size)
    chunks = [np.arange(batch_size) + r * batch_size for r in range(num_chunks)]
    expected = np.array([inputs[chunk[chunk < inputs.size]].sum() for chunk in chunks])

    vrt_xml = f"""<VRTDataset subclass='VRTProcessedDataset'>
            <Input>
                <SourceFilename>{src_filename}</SourceFilename>
            </Input>
            <ProcessingSteps>
                <Step>
                    <Algorithm>Expression</Algorithm>
                    <Argument name="expression">sum(BANDS)</Argument>
                    <Argument name="dialect">muparser</Argument>
                    <Argument name="batch_size">{batch_size}</Argument>
                </Step>
            </ProcessingSteps>
            <OutputBands count="FROM_LAST_STEP" dataType="Float32" />
            </VRTDataset>
                """

    with gdal.Open(vrt_xml) as ds:
        actual = ds.ReadAsArray().flatten()

    np.testing.assert_equal(actual, expected)


###############################################################################
# Test that serialization (for example due to statistics computation) properly
# works


def test_vrtprocesseddataset_serialize(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 1, 1)
    src_ds.GetRasterBand(1).WriteArray(np.array([[1, 2]]))
    src_ds.Close()

    vrt_filename = str(tmp_vsimem / "the.vrt")
    content = f"""<VRTDataset subclass='VRTProcessedDataset'>
    <VRTRasterBand subClass='VRTProcessedRasterBand' dataType='Byte'/>
    <Input unscale="true">
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step name="Affine combination of band values">
            <Algorithm>BandAffineCombination</Algorithm>
            <Argument name="coefficients_1">10,1</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """
    with gdaltest.tempfile(vrt_filename, content):
        ds = gdal.Open(vrt_filename)
        np.testing.assert_equal(ds.GetRasterBand(1).ReadAsArray(), np.array([[11, 12]]))
        assert ds.GetRasterBand(1).GetStatistics(False, False) is None
        ds.GetRasterBand(1).ComputeStatistics(False)
        ds.Close()

        ds = gdal.Open(vrt_filename)
        np.testing.assert_equal(ds.GetRasterBand(1).ReadAsArray(), np.array([[11, 12]]))
        assert ds.GetRasterBand(1).GetStatistics(False, False) == [
            11.0,
            12.0,
            11.5,
            0.5,
        ]


###############################################################################
# Test OutputBands


def test_vrtprocesseddataset_OutputBands():

    with gdal.Open("data/vrt/processed_OutputBands_FROM_LAST_STEP.vrt") as ds:
        assert ds.RasterCount == 2
        assert (ds.GetRasterBand(1).GetMinimum(), ds.GetRasterBand(1).GetMaximum()) == (
            None,
            None,
        )
        assert (ds.GetRasterBand(2).GetMinimum(), ds.GetRasterBand(2).GetMaximum()) == (
            None,
            None,
        )
        assert ds.GetRasterBand(1).ComputeRasterMinMax() == (84, 265)
        assert ds.GetRasterBand(2).ComputeRasterMinMax() == (94, 275)

    with gdal.Open(
        "data/vrt/processed_OutputBands_FROM_LAST_STEP_with_stats.vrt"
    ) as ds:
        assert ds.RasterCount == 2
        assert (ds.GetRasterBand(1).GetMinimum(), ds.GetRasterBand(1).GetMaximum()) == (
            84,
            265,
        )
        assert (ds.GetRasterBand(2).GetMinimum(), ds.GetRasterBand(2).GetMaximum()) == (
            94,
            275,
        )

    with gdal.Open(
        "data/vrt/processed_OutputBands_FROM_LAST_STEP_with_stats_missing_band.vrt"
    ) as ds:
        assert ds.RasterCount == 2
        assert (ds.GetRasterBand(1).GetMinimum(), ds.GetRasterBand(1).GetMaximum()) == (
            None,
            None,
        )
        assert (ds.GetRasterBand(2).GetMinimum(), ds.GetRasterBand(2).GetMaximum()) == (
            None,
            None,
        )
        assert ds.GetRasterBand(1).ComputeRasterMinMax() == (84, 265)
        assert ds.GetRasterBand(2).ComputeRasterMinMax() == (94, 275)

    with pytest.raises(Exception, match="Argument coefficients_2 is missing"):
        gdal.Open("data/vrt/processed_OutputBands_FROM_LAST_STEP_with_stats_error.vrt")

    with gdal.Open("data/vrt/processed_OutputBands_FROM_SOURCE.vrt") as ds:
        assert ds.RasterCount == 1
        assert ds.GetRasterBand(1).ComputeRasterMinMax() == (84, 255)

    with pytest.raises(
        Exception,
        match="Final step expect 1 bands, but only 2 coefficient_XX are provided",
    ):
        gdal.Open("data/vrt/processed_OutputBands_FROM_SOURCE_wrong_band_count.vrt")

    with gdal.Open("data/vrt/processed_OutputBands_USER_PROVIDED.vrt") as ds:
        assert ds.RasterCount == 1
        assert ds.GetRasterBand(1).DataType == gdal.GDT_Float32
        assert ds.GetRasterBand(1).ComputeRasterMinMax() == (84, 265)

    with pytest.raises(
        Exception,
        match="Invalid band count",
    ):
        gdal.Open("data/vrt/processed_OutputBands_USER_PROVIDED_too_large_count.vrt")

    with pytest.raises(
        Exception,
        match="Invalid value for OutputBands.count",
    ):
        gdal.Open("data/vrt/processed_OutputBands_USER_PROVIDED_non_numeric_count.vrt")

    with pytest.raises(
        Exception,
        match="Invalid value for OutputBands.dataType",
    ):
        gdal.Open("data/vrt/processed_OutputBands_USER_PROVIDED_invalid_type.vrt")


###############################################################################
# Test VRTProcessedDataset::RasterIO()


def test_vrtprocesseddataset_RasterIO(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 3, 4)
    src_ds.GetRasterBand(1).WriteArray(np.array([[1, 2], [3, 4], [5, 6]]))
    src_ds.GetRasterBand(2).WriteArray(np.array([[7, 8], [9, 10], [11, 12]]))
    src_ds.GetRasterBand(3).WriteArray(np.array([[13, 14], [15, 16], [17, 18]]))
    src_ds.GetRasterBand(4).WriteArray(np.array([[19, 20], [21, 22], [23, 24]]))
    src_ds.BuildOverviews("NEAR", [2])
    src_ds = None

    vrt_content = f"""<VRTDataset subclass='VRTProcessedDataset'>
    <Input>
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step name="Affine combination of band values">
            <Algorithm>BandAffineCombination</Algorithm>
            <Argument name="coefficients_1">0,0,1,0,0</Argument>
            <Argument name="coefficients_2">0,0,0,1,0</Argument>
            <Argument name="coefficients_3">0,0,0,0,1</Argument>
            <Argument name="coefficients_4">0,1,0,0,0</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>
        """

    ds = gdal.Open(vrt_content)
    assert ds.RasterXSize == 2
    assert ds.RasterYSize == 3
    assert ds.RasterCount == 4

    # Optimized code path with INTERLEAVE=BAND
    np.testing.assert_equal(
        ds.ReadAsArray(),
        np.array(
            [
                [[7, 8], [9, 10], [11, 12]],
                [[13, 14], [15, 16], [17, 18]],
                [[19, 20], [21, 22], [23, 24]],
                [[1, 2], [3, 4], [5, 6]],
            ]
        ),
    )

    # Optimized code path with INTERLEAVE=BAND but buf_type != native type
    np.testing.assert_equal(
        ds.ReadAsArray(buf_type=gdal.GDT_Int16),
        np.array(
            [
                [[7, 8], [9, 10], [11, 12]],
                [[13, 14], [15, 16], [17, 18]],
                [[19, 20], [21, 22], [23, 24]],
                [[1, 2], [3, 4], [5, 6]],
            ]
        ),
    )

    # Optimized code path with INTERLEAVE=BAND
    np.testing.assert_equal(
        ds.ReadAsArray(1, 2, 1, 1),
        np.array([[[12]], [[18]], [[24]], [[6]]]),
    )

    # Optimized code path with INTERLEAVE=PIXEL
    np.testing.assert_equal(
        ds.ReadAsArray(interleave="PIXEL"),
        np.array(
            [
                [[7, 13, 19, 1], [8, 14, 20, 2]],
                [[9, 15, 21, 3], [10, 16, 22, 4]],
                [[11, 17, 23, 5], [12, 18, 24, 6]],
            ]
        ),
    )

    # Optimized code path with INTERLEAVE=PIXEL but buf_type != native type
    np.testing.assert_equal(
        ds.ReadAsArray(interleave="PIXEL", buf_type=gdal.GDT_Int16),
        np.array(
            [
                [[7, 13, 19, 1], [8, 14, 20, 2]],
                [[9, 15, 21, 3], [10, 16, 22, 4]],
                [[11, 17, 23, 5], [12, 18, 24, 6]],
            ]
        ),
    )

    # Optimized code path with INTERLEAVE=PIXEL
    np.testing.assert_equal(
        ds.ReadAsArray(1, 2, 1, 1, interleave="PIXEL"),
        np.array([[[12, 18, 24, 6]]]),
    )

    # Not optimized INTERLEAVE=BAND because not enough bands
    np.testing.assert_equal(
        ds.ReadAsArray(band_list=[1, 2, 3]),
        np.array(
            [
                [[7, 8], [9, 10], [11, 12]],
                [[13, 14], [15, 16], [17, 18]],
                [[19, 20], [21, 22], [23, 24]],
            ]
        ),
    )

    # Not optimized INTERLEAVE=BAND because of out-of-order band list
    np.testing.assert_equal(
        ds.ReadAsArray(band_list=[4, 1, 2, 3]),
        np.array(
            [
                [[1, 2], [3, 4], [5, 6]],
                [[7, 8], [9, 10], [11, 12]],
                [[13, 14], [15, 16], [17, 18]],
                [[19, 20], [21, 22], [23, 24]],
            ]
        ),
    )

    # Not optimized INTERLEAVE=PIXEL because of out-of-order band list
    np.testing.assert_equal(
        ds.ReadAsArray(interleave="PIXEL", band_list=[4, 1, 2, 3]),
        np.array(
            [
                [[1, 7, 13, 19], [2, 8, 14, 20]],
                [[3, 9, 15, 21], [4, 10, 16, 22]],
                [[5, 11, 17, 23], [6, 12, 18, 24]],
            ]
        ),
    )

    # Optimized code path with overviews
    assert ds.GetRasterBand(1).GetOverview(0).XSize == 1
    assert ds.GetRasterBand(1).GetOverview(0).YSize == 2
    np.testing.assert_equal(
        ds.ReadAsArray(buf_xsize=1, buf_ysize=2),
        np.array([[[7], [11]], [[13], [17]], [[19], [23]], [[1], [5]]]),
    )

    # Non-optimized code path with overviews
    np.testing.assert_equal(
        ds.ReadAsArray(buf_xsize=1, buf_ysize=1),
        np.array([[[11]], [[17]], [[23]], [[5]]]),
    )

    # Test buffer splitting
    with gdal.config_option("VRT_PROCESSED_DATASET_ALLOWED_RAM_USAGE", "96"):
        ds = gdal.Open(vrt_content)

    # Optimized code path with INTERLEAVE=BAND
    np.testing.assert_equal(
        ds.ReadAsArray(),
        np.array(
            [
                [[7, 8], [9, 10], [11, 12]],
                [[13, 14], [15, 16], [17, 18]],
                [[19, 20], [21, 22], [23, 24]],
                [[1, 2], [3, 4], [5, 6]],
            ]
        ),
    )

    # I/O error
    gdal.GetDriverByName("GTiff").Create(
        src_filename, 1024, 1024, 4, options=["TILED=YES"]
    )
    f = gdal.VSIFOpenL(src_filename, "rb+")
    gdal.VSIFTruncateL(f, 4096)
    gdal.VSIFCloseL(f)

    ds = gdal.Open(vrt_content)

    # Error in INTERLEAVE=BAND optimized code path
    with pytest.raises(Exception):
        ds.ReadAsArray()

    # Error in INTERLEAVE=PIXEL optimized code path
    with pytest.raises(Exception):
        ds.ReadAsArray(interleave="PIXEL")

    with gdal.config_option("VRT_PROCESSED_DATASET_ALLOWED_RAM_USAGE", "96"):
        ds = gdal.Open(vrt_content)
        assert ds.GetRasterBand(1).GetBlockSize() == [1, 1]
        with pytest.raises(Exception):
            ds.ReadAsArray()


###############################################################################
# Validate processed datasets according to xsd


@pytest.mark.parametrize(
    "fname",
    [
        f
        for f in os.listdir(os.path.join(os.path.dirname(__file__), "data/vrt"))
        if f.startswith("processed")
    ],
)
def test_vrt_processeddataset_validate(fname):
    with open(os.path.join("data/vrt", fname)) as f:
        _validate(f.read())


###############################################################################
# Test reading input datasets with scale and offset


@pytest.mark.parametrize(
    "input_scaled", (True, False), ids=lambda x: f"input scaled={x}"
)
@pytest.mark.parametrize("unscale", (True, False, "auto"), ids=lambda x: f"unscale={x}")
@pytest.mark.parametrize(
    "dtype", (gdal.GDT_Int16, gdal.GDT_Float32), ids=gdal.GetDataTypeName
)
def test_vrtprocesseddataset_scaled_inputs(tmp_vsimem, input_scaled, dtype, unscale):

    src_filename = tmp_vsimem / "src.tif"

    nx = 2
    ny = 3
    nz = 2

    if dtype == gdal.GDT_Float32:
        nodata = float("nan")
    else:
        nodata = 99

    np_type = gdal_array.GDALTypeCodeToNumericTypeCode(dtype)

    data = np.arange(nx * ny * nz, dtype=np_type).reshape(nz, ny, nx)
    data[:, 2, 1] = nodata

    if input_scaled:
        offsets = [i + 2 for i in range(nz)]
        scales = [(i + 1) / 4 for i in range(nz)]
    else:
        offsets = [0 for i in range(nz)]
        scales = [1 for i in range(nz)]

    with gdal.GetDriverByName("GTiff").Create(
        src_filename, nx, ny, nz, eType=dtype
    ) as src_ds:
        src_ds.WriteArray(data)
        for i in range(src_ds.RasterCount):
            bnd = src_ds.GetRasterBand(i + 1)
            bnd.SetOffset(offsets[i])
            bnd.SetScale(scales[i])
            bnd.SetNoDataValue(nodata)

    ds = gdal.Open(
        f"""
    <VRTDataset subclass='VRTProcessedDataset'>
    <Input unscale="{unscale}">
        <SourceFilename>{src_filename}</SourceFilename>
    </Input>
    <ProcessingSteps>
        <Step>
            <Algorithm>BandAffineCombination</Algorithm>
            <Argument name="coefficients_1">0,1,0</Argument>
            <Argument name="coefficients_2">0,0,1</Argument>
        </Step>
    </ProcessingSteps>
    </VRTDataset>"""
    )

    assert ds.RasterCount == nz

    if unscale is True or (unscale == "auto" and input_scaled):
        for i in range(ds.RasterCount):
            bnd = ds.GetRasterBand(i + 1)
            assert bnd.DataType == gdal.GDT_Float64
            assert bnd.GetScale() in (None, 1)
            assert bnd.GetOffset() in (None, 0)
    else:
        for i in range(ds.RasterCount):
            bnd = ds.GetRasterBand(i + 1)
            assert bnd.DataType == dtype
            assert bnd.GetScale() == scales[i]
            assert bnd.GetOffset() == offsets[i]
            assert (
                np.isnan(bnd.GetNoDataValue())
                if np.isnan(nodata)
                else bnd.GetNoDataValue() == nodata
            )

    result = np.ma.stack(
        [ds.GetRasterBand(i + 1).ReadAsMaskedArray() for i in range(ds.RasterCount)]
    )

    if unscale:
        expected = np.ma.masked_array(
            np.stack([data[i, :, :] * scales[i] + offsets[i] for i in range(nz)]),
            np.isnan(data) if np.isnan(nodata) else data == nodata,
        )
    else:
        expected = np.ma.masked_array(
            data, np.isnan(data) if np.isnan(nodata) else data == nodata
        )

    np.testing.assert_array_equal(result.mask, expected.mask)
    np.testing.assert_array_equal(result[~result.mask], expected[~expected.mask])
