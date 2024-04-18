#!/usr/bin/env pytest
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VRTProcessedDataset support.
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
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

import gdaltest
import pytest

from osgeo import gdal

np = pytest.importorskip("numpy")
pytest.importorskip("osgeo.gdal_array")

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


def test_vrtprocesseddataset_affine_combination_nominal(tmp_vsimem):

    src_filename = str(tmp_vsimem / "src.tif")
    src_ds = gdal.GetDriverByName("GTiff").Create(src_filename, 2, 1, 3)
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
    src_ds.SetGeoTransform([0, 0.5, 0, 0, 0, 0.5])
    src_ds.Close()

    gain_filename = str(tmp_vsimem / "gain.tif")
    gain_ds = gdal.GetDriverByName("GTiff").Create(gain_filename, 3, 1, 1)
    gain_ds.GetRasterBand(1).WriteArray(np.array([[2, 4, 6]]))
    gain_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
    gain_ds.Close()

    offset_filename = str(tmp_vsimem / "offset.tif")
    offset_ds = gdal.GetDriverByName("GTiff").Create(offset_filename, 3, 1, 1)
    offset_ds.GetRasterBand(1).WriteArray(np.array([[1, 2, 3]]))
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
    <Input>
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
        assert ds.GetRasterBand(1).GetStatistics(False, False) == [0.0, 0.0, 0.0, -1.0]
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
