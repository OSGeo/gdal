#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  OpenFileGDB raster driver testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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

import struct

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("OpenFileGDB")


###############################################################################
# Open dataset with subdatasets


def test_openfilegb_raster_subdatasets():

    ds = gdal.Open("data/filegdb/gdal_test_data.gdb.zip")
    assert ds
    assert len(ds.GetSubDatasets()) == 44
    ds = gdal.Open(ds.GetSubDatasets()[0][0])
    assert ds
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20

    ds = gdal.OpenEx(
        "data/filegdb/gdal_test_data.gdb.zip", gdal.OF_RASTER | gdal.OF_VECTOR
    )
    assert ds

    with pytest.raises(Exception):
        gdal.OpenEx("data/filegdb/gdal_test_data.gdb.zip", gdal.OF_VECTOR)


###############################################################################
# Test various band types


@pytest.mark.parametrize(
    "name,datatype,checksum",
    [
        ("byte_lz77", gdal.GDT_Byte, 4672),
        ("byte_lzw", gdal.GDT_Byte, 4672),  # no compression actually
        ("uint16_lz77", gdal.GDT_UInt16, 4672),
        ("uint16_lzw", gdal.GDT_UInt16, 4672),  # no compression actually
        ("int16_lz77", gdal.GDT_Int16, 4672),
        ("int16_lzw", gdal.GDT_Int16, 4672),  # no compression actually
        ("uint32_lz77", gdal.GDT_UInt32, 4672),
        ("uint32_lzw", gdal.GDT_UInt32, 4672),  # no compression actually
        ("int32_lz77", gdal.GDT_Int32, 4672),
        ("int32_lzw", gdal.GDT_Int32, 4672),  # no compression actually
        ("float32_lz77", gdal.GDT_Float32, 4672),
        ("float32_lzw", gdal.GDT_Float32, 4672),  # no compression actually
        ("float64_lz77", gdal.GDT_Float64, 4672),
    ],
)
def test_openfilegb_raster_band_types(name, datatype, checksum):

    ds = gdal.Open("OpenFileGDB:data/filegdb/gdal_test_data.gdb.zip:" + name)
    assert ds
    assert ds.GetRasterBand(1).DataType == datatype
    assert ds.GetRasterBand(1).Checksum() == checksum


###############################################################################
# Test mask band


def test_openfilegb_raster_mask_band():

    ds = gdal.Open("OpenFileGDB:data/filegdb/gdal_test_data.gdb.zip:byte_lz77")
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4873
    assert ds.GetRasterBand(1).Checksum() == 4672

    ds = gdal.Open("OpenFileGDB:data/filegdb/gdal_test_data.gdb.zip:byte_lz77")
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 4873


###############################################################################
# Test multi band


def test_openfilegb_raster_multi_band():

    ds = gdal.Open("OpenFileGDB:data/filegdb/gdal_test_data.gdb.zip:small_world_lz77")
    assert ds.RasterXSize == 400
    assert ds.RasterYSize == 200
    assert ds.RasterCount == 3
    assert ds.GetMetadata("IMAGE_STRUCTURE") == {
        "COMPRESSION": "DEFLATE",
        "INTERLEAVE": "BAND",
    }
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
        30111,
        32302,
        40026,
    ]
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 64269
    assert ds.GetRasterBand(2).GetMaskBand().Checksum() == 64269
    assert ds.GetRasterBand(3).GetMaskBand().Checksum() == 64269
    band = ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 3
    assert band.GetOverview(-1) is None
    assert band.GetOverview(3) is None
    assert band.GetOverview(0).Checksum() == 7309
    assert band.GetOverview(0).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert band.GetOverview(0).GetMaskBand().Checksum() == 48827
    band = ds.GetRasterBand(2)
    assert band.GetOverviewCount() == 3
    assert band.GetOverview(-1) is None
    assert band.GetOverview(3) is None
    assert band.GetOverview(0).Checksum() == 7850
    assert band.GetOverview(0).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert band.GetOverview(0).GetMaskBand().Checksum() == 48827


###############################################################################
# Test 1-bit depth


def test_openfilegb_raster_one_bit():

    ds = gdal.Open("data/filegdb/dem_1bit_ScalePixelValue.gdb")
    assert ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "1"
    assert ds.GetRasterBand(1).Checksum() == 17197
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 29321


###############################################################################
# Test 4-bit depth and a rasterband_id == 2


def test_openfilegb_raster_four_bit():

    ds = gdal.Open("data/filegdb/lu_4bit.gdb")
    assert ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") == "4"
    assert ds.GetRasterBand(1).Checksum() == 40216
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 42040


###############################################################################
# Test JPEG compression


def test_openfilegb_raster_jpeg():

    ds = gdal.Open("OpenFileGDB:data/filegdb/gdal_test_data.gdb.zip:small_world_jpeg")
    assert ds.RasterXSize == 400
    assert ds.RasterYSize == 200
    assert ds.RasterCount == 3


@pytest.mark.skipif(
    gdal.GetDriverByName("JPEG") is not None,
    reason="Test specific when JPEG driver is absent",
)
def test_openfilegb_raster_jpeg_driver_not_available():

    ds = gdal.Open("OpenFileGDB:data/filegdb/gdal_test_data.gdb.zip:small_world_jpeg")
    with gdal.quiet_errors():
        assert ds.GetRasterBand(1).Checksum() == -1


@pytest.mark.require_driver("JPEG")
def test_openfilegb_raster_jpeg_read_data():

    ds = gdal.Open("OpenFileGDB:data/filegdb/gdal_test_data.gdb.zip:small_world_jpeg")
    assert ds.GetMetadata("IMAGE_STRUCTURE") == {
        "COMPRESSION": "JPEG",
        "INTERLEAVE": "BAND",
        "JPEG_QUALITY": "75",
    }
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] == [
        23495,
        18034,
        36999,
    ]
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 64269


###############################################################################
# Test JPEG2000 compression


def test_openfilegb_raster_jpeg2000():

    ds = gdal.Open(
        "OpenFileGDB:data/filegdb/gdal_test_data.gdb.zip:small_world_jpeg2000"
    )
    assert ds.RasterXSize == 400
    assert ds.RasterYSize == 200
    assert ds.RasterCount == 3
    assert ds.GetMetadata("IMAGE_STRUCTURE") == {
        "COMPRESSION": "JPEG2000",
        "INTERLEAVE": "BAND",
    }


@pytest.mark.require_driver("JP2OpenJPEG")
def test_openfilegb_raster_jpeg2000_read_data():

    ds = gdal.Open(
        "OpenFileGDB:data/filegdb/gdal_test_data.gdb.zip:small_world_jpeg2000"
    )
    assert [ds.GetRasterBand(i + 1).Checksum() for i in range(3)] != [-1, -1, -1]
    assert ds.GetRasterBand(1).GetMaskBand().Checksum() == 64269


###############################################################################
# Open dataset with Int8 data type


def test_openfilegb_raster_int8():

    ds = gdal.Open("data/filegdb/int8.gdb")
    assert ds
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetGeoTransform() == (440720, 60, 0, 3751320, 0, -60)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert ds.GetMetadata("IMAGE_STRUCTURE") == {"COMPRESSION": "DEFLATE"}
    assert ds.GetMetadata("xml:definition")[0].startswith("<DERasterDataset ")
    assert ds.GetMetadata("xml:documentation")[0].startswith("<metadata ")
    band = ds.GetRasterBand(1)
    assert band.DataType == gdal.GDT_Int8
    assert band.Checksum() == 1046
    assert band.ComputeRasterMinMax(False) == (-124, 123)
    assert band.GetMaskFlags() == gdal.GMF_PER_DATASET
    assert band.GetMaskBand().ComputeRasterMinMax(False) == (255, 255)
    assert band.GetDefaultRAT() is None
    assert band.GetMetadata() == {}


###############################################################################
# Open dataset with statistics in fras_aux_ table


def test_openfilegb_raster_statistics():

    ds = gdal.Open("data/filegdb/NCH_ES_WATER_LOGGING_HAZARD.gdb")
    assert ds
    assert ds.RasterXSize == 18731
    assert ds.RasterYSize == 19320
    assert ds.RasterCount == 1
    band = ds.GetRasterBand(1)
    assert band.DataType == gdal.GDT_Float32
    assert band.GetMetadata() == {
        "STATISTICS_COVARIANCES": "14.47671828699574",
        "STATISTICS_MAXIMUM": "23.318840026855",
        "STATISTICS_MEAN": "9.685811623136",
        "STATISTICS_MINIMUM": "-20.907217025757",
        "STATISTICS_SKIPFACTORX": "1",
        "STATISTICS_SKIPFACTORY": "1",
        "STATISTICS_STDDEV": "3.804828286138",
    }
    assert band.GetNoDataValue() == pytest.approx(3.4e38, rel=1e-7)
    assert band.GetMaskFlags() == gdal.GMF_NODATA
    assert band.GetOverviewCount() == 6
    assert band.GetOverview(-1) is None
    assert band.GetOverview(6) is None
    assert band.ReadRaster(0, 0, 1, 1) == struct.pack("f", band.GetNoDataValue())


###############################################################################
# Open dataset with RAT


def test_openfilegb_raster_rat():

    ds = gdal.Open("data/filegdb/rat.gdb")
    assert ds
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetGeoTransform() == (440720, 60, 0, 3751320, 0, -60)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26711"
    assert ds.GetMetadata("IMAGE_STRUCTURE") == {"COMPRESSION": "DEFLATE"}
    band = ds.GetRasterBand(1)
    assert band.DataType == gdal.GDT_Int8
    assert band.Checksum() == 1046

    # Get RAT
    rat = band.GetDefaultRAT()
    assert rat
    assert rat.GetColumnCount() == 3
    assert rat.GetRowCount() == 18

    # Get RAT again
    rat = band.GetDefaultRAT()
    assert rat
    assert rat.GetColumnCount() == 3
    assert rat.GetRowCount() == 18

    # Not completely sure if the GDAL API guarantees that a cloned RAT is
    # standalone, but that's how we have implemented it
    rat = rat.Clone()
    assert rat

    ds = None

    assert rat.GetTableType() == gdal.GRTT_THEMATIC
    assert rat.GetColumnCount() == 3
    assert rat.GetRowCount() == 18

    assert rat.GetNameOfCol(-1) is None
    assert rat.GetNameOfCol(0) == "Value"
    assert rat.GetNameOfCol(1) == "Count"
    assert rat.GetNameOfCol(2) == "description"
    assert rat.GetNameOfCol(3) is None

    assert rat.GetUsageOfCol(-1) == gdal.GFU_Generic
    assert rat.GetUsageOfCol(0) == gdal.GFU_MinMax
    assert rat.GetUsageOfCol(1) == gdal.GFU_PixelCount
    assert rat.GetUsageOfCol(2) == gdal.GFU_Generic
    assert rat.GetUsageOfCol(3) == gdal.GFU_Generic

    assert rat.GetColOfUsage(gdal.GFU_MinMax) == 0
    assert rat.GetColOfUsage(gdal.GFU_PixelCount) == 1
    assert rat.GetColOfUsage(gdal.GFU_Generic) == -1

    assert rat.GetTypeOfCol(-1) == gdal.GFT_Integer  # just testing it doesn't crash
    assert rat.GetTypeOfCol(0) == gdal.GFT_Integer
    assert rat.GetTypeOfCol(1) == gdal.GFT_Real
    assert rat.GetTypeOfCol(2) == gdal.GFT_String
    assert rat.GetTypeOfCol(3) == gdal.GFT_Integer  # just testing it doesn't crash

    assert rat.GetValueAsString(-1, 0) == ""
    with pytest.raises(Exception):
        assert rat.GetValueAsString(0, -1) == ""
    assert rat.GetValueAsString(0, 0) == "-124"
    assert rat.GetValueAsString(1, 0) == "-116"
    assert rat.GetValueAsString(0, 1) == "72"
    assert rat.GetValueAsString(0, 2) == "test"
    assert rat.GetValueAsString(0, 3) == ""

    assert rat.GetValueAsInt(-1, 0) == 0
    with pytest.raises(Exception):
        assert rat.GetValueAsInt(0, -1) == 0
    assert rat.GetValueAsInt(0, 0) == -124
    assert rat.GetValueAsInt(1, 0) == -116
    assert rat.GetValueAsInt(0, 1) == 72
    assert rat.GetValueAsInt(0, 2) == 0
    assert rat.GetValueAsInt(0, 3) == 0

    assert rat.GetValueAsDouble(-1, 0) == 0
    with pytest.raises(Exception):
        assert rat.GetValueAsDouble(0, -1) == 0
    assert rat.GetValueAsDouble(0, 0) == -124
    assert rat.GetValueAsDouble(1, 0) == -116
    assert rat.GetValueAsDouble(0, 1) == 72
    assert rat.GetValueAsDouble(0, 2) == 0
    assert rat.GetValueAsDouble(0, 3) == 0

    with pytest.raises(Exception):
        rat.SetValueAsString(0, 0, "foo")
    with pytest.raises(Exception):
        rat.SetValueAsInt(0, 1, 1)
    with pytest.raises(Exception):
        rat.SetValueAsDouble(0, 2, 1.5)
    with pytest.raises(Exception):
        rat.SetTableType(gdal.GRTT_THEMATIC)


###############################################################################
# Open dataset with block_origin_x != eminx and block_origin_y != emaxy


def test_openfilegb_shifted_origin():

    # https://gisdata.mn.gov/dataset/water-lake-bathy-shaded-relief with
    # all data removed
    ds = gdal.Open("data/filegdb/water_lake_bathy_shaded_relief_only_metadata.gdb")
    assert ds
    assert ds.RasterXSize == 98478
    assert ds.RasterYSize == 117334
    assert ds.GetGeoTransform() == (229052.5, 5.0, 0.0, 5404027.5, 0.0, -5.0)
    assert ds.GetSpatialRef() is None


###############################################################################
# Test opening a FileGDB v9 raster


@pytest.mark.require_curl()
def test_openfilegb_v9():

    filename = "/vsicurl/https://resources.gisdata.mn.gov/pub/data/elevation/lidar/county/washington/geodatabase/3542-23-32.gdb.zip"
    if gdal.VSIStatL(filename) is None:
        pytest.skip(f"cannot access {filename}")

    filename = "/vsizip/" + filename
    gdal.ErrorReset()
    ds = gdal.Open(filename)
    assert ds
    assert gdal.GetLastErrorMsg() == ""
    assert ds.RasterXSize == 2552
    assert ds.RasterYSize == 3612
    assert ds.GetGeoTransform() == (497481, 1, 0, 5017752, 0, -1)
    srs = ds.GetSpatialRef()
    assert "NAD83 / UTM zone 15N" in srs.GetName()

    ds = gdal.Open(f'OpenFileGDB:"{filename}":dem_1m_m')
    assert ds.RasterXSize == 2552
    assert ds.RasterYSize == 3612

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR)
    assert set([ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount())]) == set(
        [
            "contour_10f_3m",
            "building_loc_py",
            "bare_earth_pt",
            "contour_50f_3m",
            "contour_2f_3m",
            "brkln_hydro_py",
        ]
    )
