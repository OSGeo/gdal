#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for GeoTIFF format.
# Author:   TileDB, Inc
#
###############################################################################
# Copyright (c) 2019, TileDB, Inc
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

import math
import os

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("TileDB")


@pytest.mark.parametrize("mode", ["BAND", "PIXEL"])
def test_tiledb_write_complex(tmp_path, mode):
    gdaltest.tiledb_drv = gdal.GetDriverByName("TileDB")

    src_ds = gdal.Open("../gcore/data/cfloat64.tif")

    options = ["INTERLEAVE=%s" % (mode), "CREATE_GROUP=NO"]

    dsname = str(tmp_path / "tiledb_complex64")
    new_ds = gdaltest.tiledb_drv.CreateCopy(dsname, src_ds, options=options)
    meta = new_ds.GetMetadata("IMAGE_STRUCTURE")
    assert meta["INTERLEAVE"] == mode, "Did not get expected mode"
    assert meta["DATASET_TYPE"] == "raster", "Did not get expected dataset type"

    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 5028, "Did not get expected checksum on still-open file"

    with pytest.raises(
        Exception, match="only supported for datasets created with CREATE_GROUP=YES"
    ):
        new_ds.BuildOverviews("NEAR", [2])

    bnd = None
    new_ds = None


@pytest.mark.parametrize("mode", ["BAND", "PIXEL", "ATTRIBUTES"])
def test_tiledb_write_custom_blocksize(tmp_path, mode):
    gdaltest.tiledb_drv = gdal.GetDriverByName("TileDB")

    src_ds = gdal.Open("../gcore/data/utmsmall.tif")

    options = ["BLOCKXSIZE=32", "BLOCKYSIZE=32", "INTERLEAVE=%s" % (mode)]

    dsname = str(tmp_path / "tiledb_custom")
    new_ds = gdaltest.tiledb_drv.CreateCopy(dsname, src_ds, options=options)
    meta = new_ds.GetMetadata("IMAGE_STRUCTURE")
    assert meta["INTERLEAVE"] == mode, "Did not get expected mode"
    assert meta["DATASET_TYPE"] == "raster", "Did not get expected dataset type"

    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 50054, "Did not get expected checksum on still-open file"
    assert bnd.GetBlockSize() == [32, 32]

    bnd = None
    new_ds = None


@pytest.mark.parametrize("mode", ["BAND", "PIXEL"])
def test_tiledb_write_update(tmp_path, mode):
    np = pytest.importorskip("numpy")

    gdaltest.tiledb_drv = gdal.GetDriverByName("TileDB")

    options = ["INTERLEAVE=%s" % (mode)]

    dsname = str(tmp_path / "tiledb_update")
    new_ds = gdaltest.tiledb_drv.Create(
        dsname, 20, 20, 1, gdal.GDT_Byte, options=options
    )
    new_ds.GetRasterBand(1).WriteArray(np.zeros((20, 20)))
    meta = new_ds.GetMetadata("IMAGE_STRUCTURE")
    assert meta["INTERLEAVE"] == mode, "Did not get expected mode"
    assert meta["DATASET_TYPE"] == "raster", "Did not get expected dataset type"
    del new_ds

    update_ds = gdal.Open(dsname, gdal.GA_Update)
    update_bnd = update_ds.GetRasterBand(1)
    # make a partial block write
    update_bnd.WriteArray(np.ones((10, 10)) * 255)
    update_bnd = None
    update_ds = None

    test_ds = gdal.Open(dsname)
    assert (
        test_ds.GetRasterBand(1).Checksum() == 1217
    ), "Did not get expected checksum on file update"
    test_ds = None


@pytest.mark.parametrize("mode", ["BAND", "PIXEL", "ATTRIBUTES"])
def test_tiledb_write_rgb(tmp_path, mode):
    gdaltest.tiledb_drv = gdal.GetDriverByName("TileDB")

    src_ds = gdal.Open("../gcore/data/rgbsmall.tif")

    options = ["INTERLEAVE=%s" % (mode)]

    dsname = str(tmp_path / "tiledb_rgb")
    new_ds = gdaltest.tiledb_drv.CreateCopy(dsname, src_ds, options=options)
    meta = new_ds.GetMetadata("IMAGE_STRUCTURE")
    assert meta["INTERLEAVE"] == mode, "Did not get expected mode"
    assert meta["DATASET_TYPE"] == "raster", "Did not get expected dataset type"
    assert new_ds.RasterCount == 3, "Did not get expected band count"
    bnd = new_ds.GetRasterBand(2)
    assert bnd.Checksum() == 21053, "Did not get expected checksum on still-open file"

    new_ds = None


@pytest.mark.parametrize("mode", ["BAND", "PIXEL"])
def test_tiledb_write_attributes(tmp_path, tmp_vsimem, mode):
    gdaltest.tiledb_drv = gdal.GetDriverByName("TileDB")

    src_ds = gdal.Open("../gcore/data/rgbsmall.tif")
    w, h, num_bands = src_ds.RasterXSize, src_ds.RasterYSize, src_ds.RasterCount

    # build attribute data in memory
    temp1_name = str(tmp_vsimem / "temp1.tif")
    gdal.GetDriverByName("GTiff").Create(temp1_name, w, h, num_bands, gdal.GDT_Int32)
    temp2_name = str(tmp_vsimem / "temp2.tif")
    gdal.GetDriverByName("GTiff").Create(temp2_name, w, h, num_bands, gdal.GDT_Float32)

    options = [
        f"TILEDB_ATTRIBUTE={temp1_name}",
        f"TILEDB_ATTRIBUTE={temp2_name}",
        "INTERLEAVE=%s" % (mode),
    ]

    dsname = str(tmp_path / "tiledb_rgb_atts")
    new_ds = gdaltest.tiledb_drv.CreateCopy(dsname, src_ds, options=options)
    assert new_ds is not None
    assert new_ds.RasterXSize == src_ds.RasterXSize
    assert new_ds.RasterYSize == src_ds.RasterYSize
    assert new_ds.RasterCount == src_ds.RasterCount
    meta = new_ds.GetMetadata("IMAGE_STRUCTURE")
    assert meta["INTERLEAVE"] == mode, "Did not get expected mode"
    assert meta["DATASET_TYPE"] == "raster", "Did not get expected dataset type"

    with gdaltest.disable_exceptions():
        new_ds = None

    # check we can open the attributes with the band as well as the pixel values
    att1_ds = gdal.OpenEx(dsname, open_options=["TILEDB_ATTRIBUTE=temp1"])
    att2_ds = gdal.OpenEx(dsname, open_options=["TILEDB_ATTRIBUTE=temp2"])
    val_ds = gdal.Open(dsname)

    meta = val_ds.GetMetadata("IMAGE_STRUCTURE")
    assert "TILEDB_ATTRIBUTE_1" in meta
    assert meta["TILEDB_ATTRIBUTE_1"] == "temp1"
    assert "TILEDB_ATTRIBUTE_2" in meta
    assert meta["TILEDB_ATTRIBUTE_2"] == "temp2"

    assert att1_ds is not None
    assert att2_ds is not None
    assert val_ds is not None

    assert att1_ds.GetRasterBand(1).DataType == gdal.GDT_Int32
    assert att2_ds.GetRasterBand(1).DataType == gdal.GDT_Float32
    assert val_ds.RasterCount == src_ds.RasterCount
    assert val_ds.GetRasterBand(1).DataType == src_ds.GetRasterBand(1).DataType

    att1_ds = None
    att2_ds = None
    val_ds = None


@pytest.mark.require_driver("HDF5")
def test_tiledb_write_subdatasets(tmp_path):
    gdaltest.tiledb_drv = gdal.GetDriverByName("TileDB")

    src_ds = gdal.Open(
        "data/tiledb_input/DeepBlue-SeaWiFS-1.0_L3_20100101_v004-20130604T131317Z.h5"
    )

    dsname = str(tmp_path / "test_sds_array")
    new_ds = gdaltest.tiledb_drv.CreateCopy(dsname, src_ds, False)

    assert new_ds is not None
    new_ds = None
    src_ds = None

    ds = gdal.Open(dsname)
    subds = ds.GetSubDatasets()
    assert len(subds) == 2
    assert subds[0][0] == f'TILEDB:"{dsname}":solar_zenith_angle'
    assert subds[1][0] == f'TILEDB:"{dsname}":viewing_zenith_angle'

    src_ds = gdal.Open(f'TILEDB:"{dsname}":viewing_zenith_angle')
    assert src_ds.GetRasterBand(1).Checksum() == 42472
    src_ds = None

    with pytest.raises(Exception):
        gdal.Open(f'TILEDB:"{dsname}":i_dont_exist')


@pytest.mark.parametrize("mode", ["BAND", "PIXEL", "ATTRIBUTES"])
def test_tiledb_write_band_meta(tmp_path, mode):
    gdaltest.tiledb_drv = gdal.GetDriverByName("TileDB")

    src_ds = gdal.Open("../gcore/data/rgbsmall.tif")

    options = ["INTERLEAVE=%s" % (mode)]

    dsname = str(tmp_path / "tiledb_meta")
    gdaltest.tiledb_drv.CreateCopy(dsname, src_ds, options=options)

    # open array in update mode
    new_ds = gdal.Open(dsname, gdal.GA_Update)

    meta = new_ds.GetMetadata("IMAGE_STRUCTURE")
    assert meta["INTERLEAVE"] == mode, "Did not get expected mode"
    assert meta["DATASET_TYPE"] == "raster", "Did not get expected dataset type"

    bnd = new_ds.GetRasterBand(1)
    bnd.SetMetadataItem("Item", "Value")

    bnd = None
    new_ds = None

    new_ds = gdal.Open(dsname)
    assert new_ds.GetRasterBand(1).GetMetadataItem("Item") == "Value"
    new_ds = None


@pytest.mark.parametrize("mode", ["BAND", "PIXEL"])
def test_tiledb_write_history(tmp_path, mode):
    np = pytest.importorskip("numpy")

    options = ["INTERLEAVE=%s" % (mode), "TILEDB_TIMESTAMP=1"]

    gdaltest.tiledb_drv = gdal.GetDriverByName("TileDB")

    dsname = str(tmp_path / "tiledb_versioning")
    new_ds = gdaltest.tiledb_drv.Create(
        dsname, 20, 20, 1, gdal.GDT_Byte, options=options
    )
    new_ds.GetRasterBand(1).WriteArray(np.zeros((20, 20)))

    meta = new_ds.GetMetadata("IMAGE_STRUCTURE")
    assert meta["INTERLEAVE"] == mode, "Did not get expected mode"
    assert meta["DATASET_TYPE"] == "raster", "Did not get expected dataset type"

    del new_ds

    ts = [2, 3, 4, 5]

    for t in ts:
        update_ds = gdal.OpenEx(
            dsname,
            gdal.GA_Update,
            open_options=["TILEDB_TIMESTAMP=%i" % (t)],
        )
        update_bnd = update_ds.GetRasterBand(1)
        update_bnd.SetMetadataItem("TILEDB_TIMESTAMP", str(t))
        data = np.ones((20, 20)) * t
        update_bnd.WriteArray(data)
        update_bnd = None
        update_ds = None

    for t in ts:
        ds = gdal.OpenEx(dsname, open_options=["TILEDB_TIMESTAMP=%i" % (t)])
        bnd = ds.GetRasterBand(1)
        assert int(bnd.GetMetadataItem("TILEDB_TIMESTAMP")) == t
        assert bnd.Checksum() == 20 * 20 * t
        bnd = None
        ds = None

    # open at a later non-existent timestamp
    ds = gdal.OpenEx(dsname, open_options=["TILEDB_TIMESTAMP=6"])
    bnd = ds.GetRasterBand(1)
    assert int(bnd.GetMetadataItem("TILEDB_TIMESTAMP")) == 5
    bnd = None
    ds = None


@pytest.mark.parametrize(
    "outputType",
    [
        gdal.GDT_Byte,
        gdal.GDT_Int8,
        gdal.GDT_UInt16,
        gdal.GDT_Int16,
        gdal.GDT_UInt32,
        gdal.GDT_Int32,
        gdal.GDT_UInt64,
        gdal.GDT_Int64,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
        gdal.GDT_CInt16,
        gdal.GDT_CInt32,
        gdal.GDT_CFloat32,
        gdal.GDT_CFloat64,
    ],
)
def test_tiledb_read_arbitrary_array(outputType, tmp_path):

    dsname = str(tmp_path / "test_tiledb_read_arbitrary_array.tiledb")
    src_ds = gdal.Open("data/byte.tif")
    with gdaltest.config_options(
        {"TILEDB_WRITE_IMAGE_STRUCTURE": "NO", "TILEDB_ATTRIBUTE": "foo"}
    ):
        gdal.Translate(dsname, src_ds, format="TileDB", outputType=outputType)

    ds = gdal.Open(dsname)
    assert ds.RasterXSize == 256
    assert ds.RasterYSize == 256
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).DataType == outputType
    assert ds.ReadRaster(0, 0, 20, 20, buf_type=outputType) == src_ds.ReadRaster(
        buf_type=outputType
    )
    ds = None


@pytest.mark.parametrize(
    "outputType, nodata, expected_nodata",
    [
        (gdal.GDT_Byte, 1, 1),
        (gdal.GDT_Byte, -1, 255),
        (gdal.GDT_Byte, 256, 255),
        (gdal.GDT_Byte, 1.5, 255),
        (gdal.GDT_Int8, -1, -1),
        (gdal.GDT_UInt16, 1, 1),
        (gdal.GDT_Int16, -1, -1),
        (gdal.GDT_UInt32, 1, 1),
        (gdal.GDT_Int32, -1, -1),
        (gdal.GDT_UInt64, 1, 1),
        (gdal.GDT_Int64, -1, -1),
        (gdal.GDT_Float32, 1.5, 1.5),
        (gdal.GDT_Float32, float("inf"), float("inf")),
        (gdal.GDT_Float32, float("-inf"), float("-inf")),
        (gdal.GDT_Float32, float("nan"), float("nan")),
        (gdal.GDT_Float64, 1.5, 1.5),
        (gdal.GDT_Float64, float("inf"), float("inf")),
        (gdal.GDT_Float64, float("-inf"), float("-inf")),
        (gdal.GDT_Float64, float("nan"), float("nan")),
        (gdal.GDT_CInt16, 1, 1),
        (gdal.GDT_CInt32, 1, 1),
        (gdal.GDT_CFloat32, 1.5, 1.5),
        (gdal.GDT_CFloat64, 1.5, 1.5),
    ],
)
def test_tiledb_write_nodata_all_types(tmp_path, outputType, nodata, expected_nodata):

    dsname = str(tmp_path / "test_tiledb_write_nodata_all_types.tiledb")

    ds = gdal.GetDriverByName("TileDB").Create(dsname, 1, 1, 1, outputType)
    if not math.isnan(nodata) and nodata != expected_nodata:
        with pytest.raises(
            Exception, match="nodata value cannot be stored in band data type"
        ):
            ds.GetRasterBand(1).SetNoDataValue(nodata)
    else:
        ds.GetRasterBand(1).SetNoDataValue(nodata)
    ds = None

    ds = gdal.Open(dsname)
    if math.isnan(nodata):
        assert math.isnan(ds.GetRasterBand(1).GetNoDataValue())
    else:
        assert ds.GetRasterBand(1).GetNoDataValue() == expected_nodata


@pytest.mark.parametrize("mode", ["BAND", "PIXEL", "ATTRIBUTES"])
def test_tiledb_write_nodata_all_modes(tmp_path, mode):

    dsname = str(tmp_path / "test_tiledb_write_nodata_all_modes.tiledb")

    ds = gdal.GetDriverByName("TileDB").Create(
        dsname, 1, 1, 2, options=["INTERLEAVE=%s" % (mode)]
    )
    for i in range(2):
        ds.GetRasterBand(i + 1).SetNoDataValue(1)
        assert ds.GetRasterBand(i + 1).GetNoDataValue() == 1
    ds = None

    ds = gdal.Open(dsname)
    for i in range(2):
        assert ds.GetRasterBand(i + 1).GetNoDataValue() == 1


def test_tiledb_write_nodata_createcopy(tmp_path):

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.GetRasterBand(1).SetNoDataValue(1)

    dsname = str(tmp_path / "test_tiledb_write_nodata_createcopy.tiledb")
    gdal.GetDriverByName("TileDB").CreateCopy(dsname, src_ds)

    ds = gdal.Open(dsname)
    assert ds.GetRasterBand(1).GetNoDataValue() == 1


def test_tiledb_write_nodata_not_identical_all_bands(tmp_path):

    dsname = str(tmp_path / "test_tiledb_write_nodata_not_identical_all_bands.tiledb")
    ds = gdal.GetDriverByName("TileDB").Create(dsname, 1, 1, 2)
    ds.GetRasterBand(1).SetNoDataValue(1)
    with pytest.raises(Exception, match="all bands should have the same nodata value"):
        ds.GetRasterBand(2).SetNoDataValue(2)


def test_tiledb_write_nodata_error_after_rasterio(tmp_path):

    dsname = str(tmp_path / "test_tiledb_write_nodata_error_after_rasterio.tiledb")
    ds = gdal.GetDriverByName("TileDB").Create(
        dsname, 1, 1, options=["CREATE_GROUP=NO"]
    )
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(1).FlushCache()
    with pytest.raises(
        Exception, match="cannot be called after pixel values have been set"
    ):
        ds.GetRasterBand(1).SetNoDataValue(1)


def test_tiledb_write_create_group(tmp_path):

    # Create dataset and add a overview level
    dsname = str(tmp_path / "test_tiledb_write_create_group.tiledb")
    ds = gdal.GetDriverByName("TileDB").Create(dsname, 1, 2)
    ds.Close()

    # Check that it resulted in an auxiliary dataset
    ds = gdal.OpenEx(dsname, gdal.OF_MULTIDIM_RASTER)
    assert set(ds.GetRootGroup().GetMDArrayNames()) == set(
        [
            "l_0",
        ]
    )
    ds.Close()

    ds = gdal.Open(dsname)
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2


def test_tiledb_write_overviews(tmp_path):

    # This dataset name must be kept short, otherwise strange I/O errors will
    # occur on Windows !
    dsname = str(tmp_path / "test.tiledb")

    src_ds = gdal.Open("data/rgbsmall.tif")
    src_ds = gdal.Translate("", src_ds, format="MEM")
    src_ds.GetRasterBand(1).SetNoDataValue(254)

    # Create dataset and add a overview level
    ds = gdal.GetDriverByName("TileDB").CreateCopy(dsname, src_ds)
    ds.BuildOverviews("NEAR", [2])
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert ds.GetRasterBand(1).GetOverview(1) is None
    ref_ds = gdal.Translate("", src_ds, format="MEM")
    ref_ds.BuildOverviews("NEAR", [2])
    assert [
        ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(ds.RasterCount)
    ] == [
        ref_ds.GetRasterBand(i + 1).GetOverview(0).Checksum()
        for i in range(ref_ds.RasterCount)
    ]
    ds.Close()

    # Check that it resulted in an auxiliary dataset
    ds = gdal.OpenEx(dsname, gdal.OF_MULTIDIM_RASTER)
    assert set(ds.GetRootGroup().GetMDArrayNames()) == set(
        [
            "l_0",
            "l_1",
        ]
    )
    ds.Close()
    ds = gdal.Open(dsname + "/l_1")
    assert ds.RasterXSize == src_ds.RasterXSize // 2
    assert ds.RasterYSize == src_ds.RasterYSize // 2
    assert ds.RasterCount == src_ds.RasterCount
    assert ds.GetRasterBand(1).GetNoDataValue() == 254
    assert ds.GetRasterBand(1).GetMetadataItem("RESAMPLING") == "NEAREST"
    assert ds.GetGeoTransform()[0] == src_ds.GetGeoTransform()[0]
    assert ds.GetGeoTransform()[1] == src_ds.GetGeoTransform()[1] * 2
    assert ds.GetGeoTransform()[2] == src_ds.GetGeoTransform()[2] * 2
    assert ds.GetGeoTransform()[3] == src_ds.GetGeoTransform()[3]
    assert ds.GetGeoTransform()[4] == src_ds.GetGeoTransform()[4] * 2
    assert ds.GetGeoTransform()[5] == src_ds.GetGeoTransform()[5] * 2
    ds.Close()

    # Check we can access the overview after re-opening
    ds = gdal.Open(dsname)
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(-1) is None
    assert ds.GetRasterBand(1).GetOverview(1) is None
    assert [
        ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(ds.RasterCount)
    ] == [
        ref_ds.GetRasterBand(i + 1).GetOverview(0).Checksum()
        for i in range(ref_ds.RasterCount)
    ]
    with pytest.raises(
        Exception, match="Cannot delete overviews in TileDB format in read-only mode"
    ):
        ds.BuildOverviews("", [])
    with pytest.raises(
        Exception, match="Cannot create overviews in TileDB format in read-only mode"
    ):
        ds.BuildOverviews("NEAR", [2])
    ds.Close()

    # Update existing overview and change to AVERAGE resampling
    ds = gdal.Open(dsname, gdal.GA_Update)
    ds.BuildOverviews("AVERAGE", [2])
    ds.Close()

    ds = gdal.Open(dsname)
    ref_ds = gdal.Translate("", src_ds, format="MEM")
    ref_ds.BuildOverviews("AVERAGE", [2])
    assert ds.GetRasterBand(1).GetOverview(0).GetMetadataItem("RESAMPLING") == "AVERAGE"
    assert [
        ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(ds.RasterCount)
    ] == [
        ref_ds.GetRasterBand(i + 1).GetOverview(0).Checksum()
        for i in range(ref_ds.RasterCount)
    ]
    ds.Close()

    # Clear overviews
    ds = gdal.Open(dsname, gdal.GA_Update)
    ds.BuildOverviews(None, [])
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    ds.Close()

    # Check there are no more overviews after reopening
    assert not os.path.exists(dsname + "/l_1")

    ds = gdal.OpenEx(dsname, gdal.OF_MULTIDIM_RASTER)
    assert ds.GetRootGroup().GetMDArrayNames() == ["l_0"]
    ds.Close()

    ds = gdal.Open(dsname)
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    ds.Close()

    # Try accessing an overview band after clearing overviews
    # (the GDAL API doesn't really promise this is safe to do in general, but
    # this is implemented in this driver)
    ds = gdal.Open(dsname, gdal.GA_Update)
    ds.BuildOverviews("NEAR", [2])
    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    ds.BuildOverviews(None, [])
    with pytest.raises(Exception, match="Dataset has been closed"):
        ovr_band.ReadRaster()
    with pytest.raises(Exception, match="Dataset has been closed"):
        ovr_band.GetDataset().ReadRaster()
    ds.Close()

    # Test adding overviews in 2 steps
    ds = gdal.Open(dsname, gdal.GA_Update)
    ds.BuildOverviews("NEAR", [2])
    ds.Close()
    ds = gdal.Open(dsname, gdal.GA_Update)
    ds.BuildOverviews("NEAR", [4])
    ref_ds = gdal.Translate("", src_ds, format="MEM")
    ref_ds.BuildOverviews("NEAR", [2, 4])
    assert ds.GetRasterBand(1).GetOverviewCount() == 2
    assert [
        ds.GetRasterBand(i + 1).GetOverview(0).Checksum() for i in range(ds.RasterCount)
    ] == [
        ref_ds.GetRasterBand(i + 1).GetOverview(0).Checksum()
        for i in range(ref_ds.RasterCount)
    ]
    assert [
        ds.GetRasterBand(i + 1).GetOverview(1).Checksum() for i in range(ds.RasterCount)
    ] == [
        ref_ds.GetRasterBand(i + 1).GetOverview(1).Checksum()
        for i in range(ref_ds.RasterCount)
    ]
    ds.Close()


def test_tiledb_write_overviews_as_geotiff(tmp_path):

    # We don't want to promote that since GDAL 3.10 because we have now native
    # support for overviews, but GeoTIFF side-car .ovr used to work until now
    # due to base PAM mechanisms. So test this

    dsname = str(tmp_path / "test.tiledb")

    src_ds = gdal.Open("data/rgbsmall.tif")
    src_ds = gdal.Translate("", src_ds, format="MEM")
    src_ds.GetRasterBand(1).SetNoDataValue(0)
    gdal.GetDriverByName("TileDB").CreateCopy(
        dsname, src_ds, options=["CREATE_GROUP=NO"]
    )

    ds = gdal.Open(dsname)
    with gdal.config_option("TILEDB_GEOTIFF_OVERVIEWS", "YES"):
        ds.BuildOverviews("NEAR", [2])
    ds.Close()

    assert os.path.exists(str(tmp_path / "test.tiledb" / "test.tdb_0.ovr"))

    ds = gdal.Open(dsname)
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    assert ds.GetRasterBand(1).GetOverview(0) is not None
    ds.Close()

    # If there are GeoTIFF .ovr, handle them through PAM even in update mode.
    ds = gdal.Open(dsname, gdal.GA_Update)
    ds.BuildOverviews(None, [])
    assert ds.GetRasterBand(1).GetOverviewCount() == 0
    ds.Close()
