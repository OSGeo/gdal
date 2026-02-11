#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test multidimensional support in HDF4 driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import struct

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("HDF4")


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Test reading HDFEOS SWATH products


def test_hdf4multidim_hdfeos_swath():

    gdaltest.download_or_skip(
        "https://gamma.hdfgroup.org/ftp/pub/outgoing/NASAHDFfiles2/eosweb/hdf4/hdfeos2-swath-wo-dimmaps/AMSR_E_L2_Ocean_B01_200206182340_A.hdf",
        "AMSR_E_L2_Ocean_B01_200206182340_A.hdf",
    )

    ds = gdal.OpenEx(
        "tmp/cache/AMSR_E_L2_Ocean_B01_200206182340_A.hdf", gdal.OF_MULTIDIM_RASTER
    )
    assert ds
    rg = ds.GetRootGroup()
    assert rg
    assert rg.GetGroupNames() == ["swaths"]
    attrs = rg.GetAttributes()
    assert attrs
    attr = rg.GetAttribute("HDFEOSVersion")
    assert attr.Read() == "HDFEOS_V2.7.2"
    swaths = rg.OpenGroup("swaths")
    assert swaths
    assert not rg.OpenGroup("foo")
    assert swaths.GetGroupNames() == ["Swath1"]
    swath1 = swaths.OpenGroup("Swath1")
    assert swath1
    assert not swaths.OpenGroup("foo")
    attrs = swath1.GetAttributes()
    assert len(attrs) == 3
    attr = swath1.GetAttribute("SoftwareRevisionDate")
    assert attr.Read()[0:-1] == "November 7, 2003"
    attr = swath1.GetAttribute("SoftwareBuildNumber")
    assert attr.Read() == 1
    dims = swath1.GetDimensions()
    assert len(dims) == 4
    dim = next((x for x in dims if x.GetName() == "DataXtrack_lo"), None)
    assert dim
    assert dim.GetFullName() == "/swaths/Swath1/DataXtrack_lo"
    assert dim.GetSize() == 196
    assert swath1.GetGroupNames() == ["Data Fields", "Geolocation Fields"]
    assert not swath1.OpenGroup("foo")
    datafields = swath1.OpenGroup("Data Fields")
    assert datafields
    assert len(datafields.GetMDArrayNames()) == 10
    assert not datafields.OpenMDArray("foo")
    array = datafields.OpenMDArray("High_res_cloud")
    assert array
    dims = array.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetFullName() == "/swaths/Swath1/DataTrack_lo"
    assert array.GetDataType().GetNumericDataType() == gdal.GDT_Int16
    attr = array.GetAttribute("Scale")
    assert attr.Read() == 9.999999747378752e-05

    got_data = array.Read(array_start_idx=[13, 0], count=[3, 2])
    assert len(got_data) == 3 * 2 * 2
    assert struct.unpack("h" * 6, got_data) == (0, 0, 17318, 17317, 17318, 17317)

    got_data = array.Read(
        array_start_idx=[13, 0],
        count=[3, 2],
        buffer_datatype=gdal.ExtendedDataType.Create(gdal.GDT_Int32),
    )
    assert len(got_data) == 3 * 2 * 4
    assert struct.unpack("i" * 6, got_data) == (0, 0, 17318, 17317, 17318, 17317)

    got_data = array.Read(array_start_idx=[13, 0], count=[3, 2], array_step=[2, 1])
    assert len(got_data) == 3 * 2 * 2
    assert struct.unpack("h" * 6, got_data) == (0, 0, 17318, 17317, 17317, 17317)

    got_data = array.Read(array_start_idx=[15, 1], count=[3, 2], array_step=[-1, -1])
    assert len(got_data) == 3 * 2 * 2
    assert struct.unpack("h" * 6, got_data) == (17317, 17318, 17317, 17318, 0, 0)

    got_data = array.Read(array_start_idx=[13, 0], count=[3, 2], buffer_stride=[1, 3])
    assert len(got_data) == 3 * 2 * 2
    assert struct.unpack("h" * 6, got_data) == (0, 17318, 17318, 0, 17317, 17317)

    ds = gdal.OpenEx(
        "tmp/cache/AMSR_E_L2_Ocean_B01_200206182340_A.hdf",
        gdal.OF_MULTIDIM_RASTER,
        open_options=["LIST_SDS=YES"],
    )
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ["swaths", "scientific_datasets"]


###############################################################################
# Test reading HDFEOS GRID products


def test_hdf4multidim_hdfeos_grid():

    gdaltest.download_or_skip(
        "http://download.osgeo.org/gdal/data/hdf4/MOD09A1.A2010041.h06v03.005.2010051001103.hdf",
        "MOD09A1.A2010041.h06v03.005.2010051001103.hdf",
    )

    ds = gdal.OpenEx(
        "tmp/cache/MOD09A1.A2010041.h06v03.005.2010051001103.hdf",
        gdal.OF_MULTIDIM_RASTER,
    )
    assert ds
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ["eos_grids"]
    attrs = rg.GetAttributes()
    assert attrs
    attr = rg.GetAttribute("HDFEOSVersion")
    assert attr.Read() == "HDFEOS_V2.9"
    eos_grids = rg.OpenGroup("eos_grids")
    assert eos_grids
    assert not rg.OpenGroup("foo")
    assert eos_grids.GetGroupNames() == ["MOD_Grid_500m_Surface_Reflectance"]
    MOD_Grid_500m_Surface_Reflectance = eos_grids.OpenGroup(
        "MOD_Grid_500m_Surface_Reflectance"
    )
    assert MOD_Grid_500m_Surface_Reflectance
    assert not eos_grids.OpenGroup("foo")
    attrs = MOD_Grid_500m_Surface_Reflectance.GetAttributes()
    assert len(attrs) == 0
    assert MOD_Grid_500m_Surface_Reflectance.GetGroupNames() == ["Data Fields"]
    assert not MOD_Grid_500m_Surface_Reflectance.OpenGroup("foo")
    datafields = MOD_Grid_500m_Surface_Reflectance.OpenGroup("Data Fields")
    assert datafields
    assert len(datafields.GetMDArrayNames()) == 13
    assert not datafields.OpenMDArray("foo")
    array = datafields.OpenMDArray("sur_refl_b01")
    assert array
    dims = array.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetFullName() == "/eos_grids/MOD_Grid_500m_Surface_Reflectance/YDim"
    assert dims[0].GetSize() == 2400
    assert dims[1].GetFullName() == "/eos_grids/MOD_Grid_500m_Surface_Reflectance/XDim"
    assert dims[1].GetSize() == 2400
    assert array.GetDataType().GetNumericDataType() == gdal.GDT_Int16
    assert array.GetNoDataValueAsDouble() == -28672.0
    assert array.GetOffset() == 0
    assert array.GetScale() == 0.0001
    assert array.GetUnit() == "reflectance"
    attr = array.GetAttribute("valid_range")
    assert attr.Read() == (-100, 16000)
    assert array.GetSpatialRef()

    got_data = array.Read(array_start_idx=[2398, 2398], count=[2, 2])
    assert len(got_data) == 2 * 2 * 2
    assert struct.unpack("h" * 4, got_data) == (-24, 0, -15, -22)

    dims = MOD_Grid_500m_Surface_Reflectance.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetName() == "YDim"
    assert dims[0].GetIndexingVariable()
    assert dims[1].GetName() == "XDim"
    assert dims[1].GetIndexingVariable()

    assert MOD_Grid_500m_Surface_Reflectance.GetMDArrayNames() == ["YDim", "XDim"]
    XDim = MOD_Grid_500m_Surface_Reflectance.OpenMDArray("XDim")
    assert XDim
    YDim = MOD_Grid_500m_Surface_Reflectance.OpenMDArray("YDim")
    assert YDim
    assert not MOD_Grid_500m_Surface_Reflectance.OpenMDArray("foo")

    ds = gdal.OpenEx(
        "tmp/cache/MOD09A1.A2010041.h06v03.005.2010051001103.hdf",
        gdal.OF_MULTIDIM_RASTER,
        open_options=["LIST_SDS=YES"],
    )
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ["eos_grids", "scientific_datasets"]


###############################################################################
# Test reading GDAL SDS 2D


def test_hdf4multidim_gdal_sds_2d():

    ds = gdal.OpenEx("data/byte_2.hdf", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert len(rg.GetGroupNames()) == 0
    assert rg.OpenGroup("scientific_datasets") is None
    assert rg.GetMDArrayNames() == ["Band0", "X", "Y"]
    dims = rg.GetDimensions()
    assert len(dims) == 2
    array = rg.OpenMDArray("Band0")
    assert array
    dims = array.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetFullName() == "/Y"
    assert dims[1].GetFullName() == "/X"
    assert array.GetDataType().GetNumericDataType() == gdal.GDT_UInt8

    got_data = array.Read(array_start_idx=[0, 0], count=[2, 2])
    assert len(got_data) == 2 * 2
    assert struct.unpack("B" * 4, got_data) == (107, 123, 115, 132)
    assert array.GetSpatialRef()
    assert array.GetUnit() == ""
    assert not array.GetOffset()
    assert not array.GetScale()
    assert not array.GetNoDataValueAsDouble()

    X = dims[0].GetIndexingVariable()
    assert X
    assert struct.unpack("d" * 2, X.Read(array_start_idx=[0], count=[2])) == (
        3751290.0,
        3751230.0,
    )

    Y = dims[1].GetIndexingVariable()
    assert Y
    assert struct.unpack("d" * 2, Y.Read(array_start_idx=[0], count=[2])) == (
        440750.0,
        440810.0,
    )


###############################################################################
# Test reading GDAL SDS 3D


def test_hdf4multidim_gdal_sds_3d():

    ds = gdal.OpenEx("data/byte_3.hdf", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert len(rg.GetGroupNames()) == 0
    assert rg.OpenGroup("scientific_datasets") is None
    assert rg.GetMDArrayNames() == ["3-dimensional Scientific Dataset", "X", "Y"]
    dims = rg.GetDimensions()
    assert len(dims) == 3
    array = rg.OpenMDArray("3-dimensional Scientific Dataset")
    assert array
    dims = array.GetDimensions()
    assert len(dims) == 3
    assert dims[0].GetFullName() == "/Y"
    assert dims[1].GetFullName() == "/X"
    assert dims[2].GetFullName() == "/Band"
    assert array.GetDataType().GetNumericDataType() == gdal.GDT_UInt8

    got_data = array.Read(array_start_idx=[0, 0, 0], count=[2, 2, 1])
    assert len(got_data) == 2 * 2
    assert struct.unpack("B" * 4, got_data) == (107, 123, 115, 132)

    got_data = array.Transpose([2, 1, 0]).Read(
        array_start_idx=[0, 0, 0], count=[1, 2, 2]
    )
    assert len(got_data) == 2 * 2
    assert struct.unpack("B" * 4, got_data) == (107, 115, 123, 132)

    assert array.GetSpatialRef()

    X = dims[0].GetIndexingVariable()
    assert X
    assert struct.unpack("d" * 2, X.Read(array_start_idx=[0], count=[2])) == (
        3751290.0,
        3751230.0,
    )
    assert rg.OpenMDArray("X")

    Y = dims[1].GetIndexingVariable()
    assert Y
    assert struct.unpack("d" * 2, Y.Read(array_start_idx=[0], count=[2])) == (
        440750.0,
        440810.0,
    )
    assert rg.OpenMDArray("Y")


###############################################################################
# Test reading a simple SDS product


def test_hdf4multidim_sds():

    # Generated with
    # https://support.hdfgroup.org/ftp/HDF/HDF_Current/src/unpacked/mfhdf/examples/SD_create_sds.c
    # + https://support.hdfgroup.org/ftp/HDF/HDF_Current/src/unpacked/mfhdf/examples/SD_set_get_dim_info.c
    # + https://support.hdfgroup.org/ftp/HDF/HDF_Current/src/unpacked/mfhdf/examples/SD_set_attr.c
    ds = gdal.OpenEx("data/SDS.hdf", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ["scientific_datasets"]
    subg = rg.OpenGroup("scientific_datasets")
    assert subg
    attrs = rg.GetAttributes()
    assert len(attrs) == 1
    assert attrs[0].GetName() == "File_contents"
    assert attrs[0].Read() == "Storm_track_data"
    assert not rg.OpenGroup("foo")
    assert not rg.GetMDArrayNames()
    assert not rg.OpenMDArray("foo")
    assert not subg.GetGroupNames()
    assert not subg.OpenGroup("foo")
    assert subg.GetMDArrayNames() == ["SDStemplate", "Y_Axis", "X_Axis"]
    dims = subg.GetDimensions()
    assert len(dims) == 2
    array = subg.OpenMDArray("SDStemplate")
    assert array
    dims = array.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetFullName() == "/scientific_datasets/Y_Axis"
    assert dims[0].GetIndexingVariable()
    assert dims[1].GetFullName() == "/scientific_datasets/X_Axis"
    assert dims[1].GetIndexingVariable()
    attrs = array.GetAttributes()
    assert len(attrs) == 1
    assert attrs[0].GetName() == "Valid_range"
    attr = array.GetAttribute("Valid_range")
    assert attr
    assert attr.Read() == (2, 10)
    assert array.GetUnit() == ""
    assert not array.GetSpatialRef()


###############################################################################
# Test reading a SDS product with unlimited dimension


def test_hdf4multidim_sds_unlimited_dim():

    # Generated with
    # hhttps://support.hdfgroup.org/ftp/HDF/HDF_Current/src/unpacked/mfhdf/examples/SD_unlimited_sds.c
    ds = gdal.OpenEx("data/SDSUNLIMITED.hdf", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ["scientific_datasets"]
    subg = rg.OpenGroup("scientific_datasets")
    assert subg
    dims = subg.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetName() == "fakeDim0"
    assert dims[0].GetSize() == 11
    array = subg.OpenMDArray("AppendableData")
    dims = array.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetName() == "fakeDim0"
    assert dims[0].GetSize() == 11
    assert len(array.Read()) == 11 * 10 * 4


###############################################################################
# Test reading a 'random' SDS product


def test_hdf4multidim_sds_read_world():

    gdaltest.download_or_skip(
        "http://download.osgeo.org/gdal/data/hdf4/A2004259075000.L2_LAC_SST.hdf",
        "A2004259075000.L2_LAC_SST.hdf",
    )

    ds = gdal.OpenEx("tmp/cache/A2004259075000.L2_LAC_SST.hdf", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ["scientific_datasets"]
    subg = rg.OpenGroup("scientific_datasets")
    assert subg
    assert not rg.OpenGroup("foo")
    assert not rg.GetMDArrayNames()
    assert not rg.OpenMDArray("foo")
    assert not subg.GetGroupNames()
    assert not subg.OpenGroup("foo")
    assert subg.GetMDArrayNames() == ["sst"]
    dims = subg.GetDimensions()
    assert len(dims) == 2
    sst = subg.OpenMDArray("sst")
    assert sst
    dims = sst.GetDimensions()
    assert len(dims) == 2
    assert dims[0].GetFullName() == "/scientific_datasets/fakeDim0"
    assert dims[1].GetFullName() == "/scientific_datasets/fakeDim1"
    attrs = sst.GetAttributes()
    assert len(attrs) == 5
    attr = sst.GetAttribute("long_name")
    assert attr
    assert attr.Read() == "Sea Surface Temperature"
    assert sst.GetUnit() == "degrees-C"
    assert not sst.GetSpatialRef()


###############################################################################
# Test reading a SDS product with indexed dimensions


def test_hdf4multidim_sds_read_world_with_indexing_variable():

    gdaltest.download_or_skip(
        "https://download.osgeo.org/gdal/data/hdf4/REANALYSIS_1999217.hdf",
        "REANALYSIS_1999217.hdf",
    )

    ds = gdal.OpenEx("tmp/cache/REANALYSIS_1999217.hdf", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ["scientific_datasets"]
    subg = rg.OpenGroup("scientific_datasets")
    assert subg
    dims = subg.GetDimensions()
    assert len(dims) == 3
    assert dims[0].GetFullName() == "/scientific_datasets/lat"
    assert dims[0].GetIndexingVariable()
    assert dims[1].GetFullName() == "/scientific_datasets/lon"
    assert dims[1].GetIndexingVariable()
    assert dims[2].GetFullName() == "/scientific_datasets/time"
    assert not dims[2].GetIndexingVariable()
    assert subg.GetMDArrayNames() == ["lat", "lon", "slp", "pr_wtr", "air"]
    slp = subg.OpenMDArray("slp")
    assert slp
    assert slp.GetUnit() == "Pascals"
    assert slp.GetOffset() == 119765.0
    assert slp.GetScale() == 1.0


###############################################################################
# Test reading a GR dataset


def test_hdf4multidim_gr():

    # Generated by https://support.hdfgroup.org/ftp/HDF/HDF_Current/src/unpacked/hdf/examples/GR_create_and_write_image.c

    ds = gdal.OpenEx("data/General_RImages.hdf", gdal.OF_MULTIDIM_RASTER)
    assert ds
    rg = ds.GetRootGroup()
    assert rg.GetGroupNames() == ["general_rasters"]
    subg = rg.OpenGroup("general_rasters")
    assert subg
    attrs = subg.GetAttributes()
    assert len(attrs) == 2
    attr = subg.GetAttribute("File Attribute 1")
    assert attr.Read() == "Contents of First FILE Attribute"
    assert not subg.GetGroupNames()
    assert not subg.OpenGroup("foo")
    assert subg.GetMDArrayNames() == ["Image Array 1"]
    array = subg.OpenMDArray("Image Array 1")
    assert not subg.OpenMDArray("foo")
    dims = array.GetDimensions()
    assert len(dims) == 3
    assert dims[0].GetName() == "y"
    assert dims[0].GetSize() == 10
    assert dims[1].GetName() == "x"
    assert dims[1].GetSize() == 5
    assert dims[2].GetName() == "bands"
    assert dims[2].GetSize() == 2

    got_data = array.Read(array_start_idx=[1, 2, 0], count=[3, 2, 2])
    assert len(got_data) == 3 * 2 * 2 * 2
    assert struct.unpack("h" * 12, got_data) == (4, 5, 5, 6, 6, 7, 5, 6, 6, 7, 7, 8)

    got_data = array.Read(array_start_idx=[1, 2, 0], count=[3, 2, 1])
    assert len(got_data) == 3 * 2 * 1 * 2
    assert struct.unpack("h" * 6, got_data) == (4, 5, 6, 5, 6, 7)

    got_data = array.Read(array_start_idx=[1, 2, 1], count=[3, 2, 1])
    assert len(got_data) == 3 * 2 * 1 * 2
    assert struct.unpack("h" * 6, got_data) == (5, 6, 7, 6, 7, 8)

    got_data = array.Read(
        array_start_idx=[1, 2, 1], count=[3, 2, 2], array_step=[1, 1, -1]
    )
    assert len(got_data) == 3 * 2 * 2 * 2
    assert struct.unpack("h" * 12, got_data) == (5, 4, 6, 5, 7, 6, 6, 5, 7, 6, 8, 7)

    attrs = array.GetAttributes()
    assert len(attrs) == 2
    attr = array.GetAttribute("Image Attribute 1")
    assert attr.Read() == "Contents of IMAGE's First Attribute"
    attr = array.GetAttribute("Image Attribute 2")
    assert attr.Read() == (1, 2, 3, 4, 5, 6)


###############################################################################
# Test reading a GR dataset with a palette


def test_hdf4multidim_gr_palette():

    # Generated by https://support.hdfgroup.org/ftp/HDF/HDF_Current/src/unpacked/hdf/examples/GR_write_palette.c

    def get_lut():
        ds = gdal.OpenEx("data/Image_with_Palette.hdf", gdal.OF_MULTIDIM_RASTER)
        assert ds
        rg = ds.GetRootGroup()
        assert rg.GetGroupNames() == ["general_rasters"]
        subg = rg.OpenGroup("general_rasters")
        array = subg.OpenMDArray("Image with Palette")
        assert array
        lut = array.GetAttribute("lut")
        return lut

    lut = get_lut()
    assert lut
    got = lut.Read()
    assert len(got) == 3 * 256
    assert got[0] == 0
    assert got[1] == 1
    assert got[2] == 2
    assert got[255 * 3 + 0] == 255
    assert got[255 * 3 + 1] == 0
    assert got[255 * 3 + 2] == 1
