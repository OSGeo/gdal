#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for HDF5 driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import array
import shutil

import gdaltest
import pytest
from uffd import uffd_compare

from osgeo import gdal

###############################################################################
# Test if HDF5 driver is present


pytestmark = pytest.mark.require_driver("HDF5")


@pytest.fixture(autouse=True)
def check_no_file_leaks():
    num_files = len(gdaltest.get_opened_files())

    yield

    diff = len(gdaltest.get_opened_files()) - num_files
    # For some weird reason, we sometimes get less files opened than at the
    # start. Cf https://github.com/OSGeo/gdal/actions/runs/11349015748/job/31564138716?pr=10896
    assert diff <= 0, "Leak of file handles: %d leaked" % diff


###############################################################################
# Confirm expected subdataset information.


def test_hdf5_2():
    ds = gdal.Open("data/hdf5/groups.h5")

    sds_list = ds.GetMetadata("SUBDATASETS")

    assert len(sds_list) == 4, "Did not get expected subdataset count."

    assert (
        sds_list["SUBDATASET_1_NAME"]
        == 'HDF5:"data/hdf5/groups.h5"://MyGroup/Group_A/dset2'
        and sds_list["SUBDATASET_2_NAME"]
        == 'HDF5:"data/hdf5/groups.h5"://MyGroup/dset1'
    ), "did not get expected subdatasets."

    ds = None

    assert not gdaltest.is_file_open("data/hdf5/groups.h5"), "file still opened."


###############################################################################
# Confirm that single variable files can be accessed directly without
# subdataset stuff.


def test_hdf5_3():

    ds = gdal.Open("data/hdf5/u8be.h5")

    band = ds.GetRasterBand(1)
    cs = band.Checksum()
    assert cs == 135, "did not get expected checksum"
    assert band.GetNoDataValue() is None
    assert band.GetOffset() is None
    assert band.GetScale() is None

    ds = None

    assert not gdaltest.is_file_open("data/hdf5/u8be.h5"), "file still opened."


###############################################################################
# Confirm subdataset access, and checksum.


def test_hdf5_4():

    ds = gdal.Open('HDF5:"data/hdf5/u8be.h5"://TestArray')

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 135, "did not get expected checksum"


###############################################################################
# Similar check on a 16bit dataset.


def test_hdf5_5():

    ds = gdal.Open('HDF5:"data/hdf5/groups.h5"://MyGroup/dset1')

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 18, "did not get expected checksum"


###############################################################################
# Test generating an overview on a subdataset.


def test_hdf5_6():

    shutil.copyfile("data/hdf5/groups.h5", "tmp/groups.h5")

    ds = gdal.Open('HDF5:"tmp/groups.h5"://MyGroup/dset1')
    ds.BuildOverviews(overviewlist=[2])
    ds = None

    assert not gdaltest.is_file_open("tmp/groups.h5"), "file still opened."

    ds = gdal.Open('HDF5:"tmp/groups.h5"://MyGroup/dset1')
    assert ds.GetRasterBand(1).GetOverviewCount() == 1, "failed to find overview"
    ds = None

    # confirm that it works with a different path. (#3290)

    ds = gdal.Open('HDF5:"data/../tmp/groups.h5"://MyGroup/dset1')
    assert (
        ds.GetRasterBand(1).GetOverviewCount() == 1
    ), "failed to find overview with alternate path"
    ovfile = ds.GetMetadataItem("OVERVIEW_FILE", "OVERVIEWS")
    assert ovfile[:11] == "data/../tmp", "did not get expected OVERVIEW_FILE."
    ds = None

    gdaltest.clean_tmp()


###############################################################################
# Coarse metadata check (regression test for #2412).


def test_hdf5_7():

    ds = gdal.Open("data/hdf5/metadata.h5")
    metadata = ds.GetMetadata()
    metadataList = ds.GetMetadata_List()
    ds = None

    assert not gdaltest.is_file_open("data/hdf5/metadata.h5"), "file still opened."

    assert len(metadata) == len(metadataList), "error in metadata dictionary setup"

    metadataList = [item.split("=", 1)[0] for item in metadataList]
    for key in metadataList:
        try:
            metadata.pop(key)
        except KeyError:
            pytest.fail('unable to find "%s" key' % key)


###############################################################################
# Test metadata names.


def test_hdf5_8():

    ds = gdal.Open("data/hdf5/metadata.h5")
    metadata = ds.GetMetadata()
    ds = None

    assert metadata, "no metadata found"

    h5groups = [
        "G1",
        "Group with spaces",
        "Group_with_underscores",
        "Group with spaces_and_underscores",
    ]
    h5datasets = [
        "D1",
        "Dataset with spaces",
        "Dataset_with_underscores",
        "Dataset with spaces_and_underscores",
    ]
    attributes = {
        "attribute": "value",
        "attribute with spaces": 0,
        "attribute_with underscores": 0,
        "attribute with spaces_and_underscores": 0.1,
    }

    def scanMetadata(parts):
        for attr in attributes:
            name = "_".join(parts + [attr])
            name = name.replace(" ", "_")
            assert name in metadata, 'unable to find metadata: "%s"' % name

            value = metadata.pop(name)

            value = value.strip(" d")
            value = type(attributes[attr])(value)
            assert (
                value == attributes[attr]
            ), 'incorrect metadata value for "%s": ' '"%s" != "%s"' % (
                name,
                value,
                attributes[attr],
            )

    # level0
    assert scanMetadata([]) is None

    # level1 datasets
    for h5dataset in h5datasets:
        assert scanMetadata([h5dataset]) is None

    # level1 groups
    for h5group in h5groups:
        assert scanMetadata([h5group]) is None

        # level2 datasets
        for h5dataset in h5datasets:
            assert scanMetadata([h5group, h5dataset]) is None


###############################################################################
# Variable length string metadata check (regression test for #4228).


def test_hdf5_9():

    if int(gdal.VersionInfo("VERSION_NUM")) < 1900:
        pytest.skip("would crash")

    ds = gdal.Open("data/hdf5/vlstr_metadata.h5")
    metadata = ds.GetRasterBand(1).GetMetadata()
    ds = None
    assert not gdaltest.is_file_open(
        "data/hdf5/vlstr_metadata.h5"
    ), "file still opened."

    ref_metadata = {
        "BANDNAMES": "SAA",
        "CODING": "0.6666666667 0.0000000000 TRUE",
        "FLAGS": "255=noValue",
        "MAPPING": "Geographic Lat/Lon 0.5000000000 0.5000000000 27.3154761905 -5.0833333333 0.0029761905 0.0029761905 WGS84 Degrees",
        "NOVALUE": "255",
        "RANGE": "0 255 0 255",
    }

    assert len(metadata) == len(
        ref_metadata
    ), "incorrect number of metadata: " "expected %d, got %d" % (
        len(ref_metadata),
        len(metadata),
    )

    for key in metadata:
        assert key in ref_metadata, 'unexpected metadata key "%s"' % key

        assert (
            metadata[key] == ref_metadata[key]
        ), 'incorrect metadata value for key "%s": ' 'expected "%s", got "%s" ' % (
            key,
            ref_metadata[key],
            metadata[key],
        )


###############################################################################
# Test CSK_DGM.h5 (#4160)


def test_hdf5_10():

    # Try opening the QLK subdataset to check that no error is generated
    gdal.ErrorReset()
    ds = gdal.Open('HDF5:"data/hdf5/CSK_DGM.h5"://S01/QLK')
    assert ds is not None and gdal.GetLastErrorMsg() == ""
    ds = None

    ds = gdal.Open('HDF5:"data/hdf5/CSK_DGM.h5"://S01/SBI')
    got_gcpprojection = ds.GetGCPProjection()
    assert got_gcpprojection.startswith('GEOGCS["WGS 84",DATUM["WGS_1984"')

    got_gcps = ds.GetGCPs()
    assert len(got_gcps) == 4

    assert (
        got_gcps[0].GCPPixel == pytest.approx(0, abs=1e-5)
        and got_gcps[0].GCPLine == pytest.approx(0, abs=1e-5)
        and got_gcps[0].GCPX == pytest.approx(12.2395902509238, abs=1e-5)
        and got_gcps[0].GCPY == pytest.approx(44.7280047434954, abs=1e-5)
    )

    ds = None
    assert not gdaltest.is_file_open("data/hdf5/CSK_DGM.h5"), "file still opened."


###############################################################################
# Test CSK_GEC.h5 (#4160)


def test_hdf5_11():

    # Try opening the QLK subdataset to check that no error is generated
    gdal.ErrorReset()
    ds = gdal.Open('HDF5:"data/hdf5/CSK_GEC.h5"://S01/QLK')
    assert ds is not None and gdal.GetLastErrorMsg() == ""
    ds = None

    ds = gdal.Open('HDF5:"data/hdf5/CSK_GEC.h5"://S01/SBI')
    got_projection = ds.GetProjection()
    assert got_projection.startswith(
        'PROJCS["Transverse_Mercator",GEOGCS["WGS 84",DATUM["WGS_1984"'
    )

    got_gt = ds.GetGeoTransform()
    expected_gt = (275592.5, 2.5, 0.0, 4998152.5, 0.0, -2.5)
    for i in range(6):
        assert got_gt[i] == pytest.approx(expected_gt[i], abs=1e-5)

    ds = None

    assert not gdaltest.is_file_open("data/hdf5/CSK_GEC.h5"), "file still opened."


###############################################################################
# Test ODIM_H5 (#5032)


def test_hdf5_12():

    gdaltest.download_or_skip(
        "http://trac.osgeo.org/gdal/raw-attachment/ticket/5032/norsa.ss.ppi-00.5-dbz.aeqd-1000.20070601T000039Z.hdf",
        "norsa.ss.ppi-00.5-dbz.aeqd-1000.20070601T000039Z.hdf",
    )

    ds = gdal.Open("tmp/cache/norsa.ss.ppi-00.5-dbz.aeqd-1000.20070601T000039Z.hdf")
    got_projection = ds.GetProjection()
    assert "Azimuthal_Equidistant" in got_projection

    got_gt = ds.GetGeoTransform()
    expected_gt = (
        -239999.9823595533,
        997.9165855496311,
        0.0,
        239000.03320328312,
        0.0,
        -997.9167782264051,
    )

    assert max([abs(got_gt[i] - expected_gt[i]) for i in range(6)]) <= 1e-5, got_gt


###############################################################################
# Test MODIS L2 HDF5 GCPs (#6666)


def test_hdf5_13():

    # Similar test file is available from
    # https://oceandata.sci.gsfc.nasa.gov/ob/getfile/AQUA_MODIS.20160929T115000.L2.OC.nc
    # Download requires NASA EarthData login, not supported by gdaltest

    gdaltest.download_or_skip(
        "http://download.osgeo.org/gdal/data/netcdf/A2016273115000.L2_LAC_OC.nc",
        "A2016273115000.L2_LAC_OC.nc",
    )

    ds = gdal.Open(
        'HDF5:"tmp/cache/A2016273115000.L2_LAC_OC.nc"://geophysical_data/Kd_490'
    )

    got_gcps = ds.GetGCPs()
    assert len(got_gcps) == 3030

    assert (
        got_gcps[0].GCPPixel == pytest.approx(0.5, abs=1e-5)
        and got_gcps[0].GCPLine == pytest.approx(0.5, abs=1e-5)
        and got_gcps[0].GCPX == pytest.approx(33.1655693, abs=1e-5)
        and got_gcps[0].GCPY == pytest.approx(39.3207207, abs=1e-5)
    )


###############################################################################
# Test complex data subsets


def test_hdf5_14():

    ds = gdal.Open("data/hdf5/complex.h5")
    sds_list = ds.GetMetadata("SUBDATASETS")

    assert len(sds_list) == 6, "Did not get expected complex subdataset count."

    assert (
        sds_list["SUBDATASET_1_NAME"] == 'HDF5:"data/hdf5/complex.h5"://f16'
        and sds_list["SUBDATASET_2_NAME"] == 'HDF5:"data/hdf5/complex.h5"://f32'
        and sds_list["SUBDATASET_3_NAME"] == 'HDF5:"data/hdf5/complex.h5"://f64'
    ), "did not get expected subdatasets."

    ds = None

    assert not gdaltest.is_file_open("data/hdf5/complex.h5"), "file still opened."


###############################################################################
# Confirm complex subset data access and checksum
# Start with Float32


def test_hdf5_15():

    ds = gdal.Open('HDF5:"data/hdf5/complex.h5"://f32')

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 523, "did not get expected checksum"


# Repeat for Float64


def test_hdf5_16():

    ds = gdal.Open('HDF5:"data/hdf5/complex.h5"://f64')

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 511, "did not get expected checksum"


# Repeat for Float16


def test_hdf5_17():

    ds = gdal.Open('HDF5:"data/hdf5/complex.h5"://f16')

    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 412, "did not get expected checksum"


def test_hdf5_single_char_varname():

    ds = gdal.Open('HDF5:"data/hdf5/single_char_varname.h5"://e')
    assert ds is not None


def test_hdf5_attr_all_datatypes():

    ds = gdal.Open("data/hdf5/attr_all_datatypes.h5")
    assert ds is not None
    assert ds.GetMetadata() == {
        "attr_float16": "125",
        "attr_float32": "125",
        "attr_float64": "125",
        "attr_int16": "125",
        "attr_int32": "125",
        "attr_int8": "125",
        "attr_uint16": "125",
        "attr_uint32": "125",
        "attr_uint8": "125",
    }


def test_hdf5_virtual_file():
    hdf5_files = [
        "hdf5/CSK_GEC.h5",
        "hdf5/vlstr_metadata.h5",
        "hdf5/groups.h5",
        "hdf5/complex.h5",
        "hdf5/single_char_varname.h5",
        "hdf5/CSK_DGM.h5",
        "hdf5/u8be.h5",
        "hdf5/metadata.h5",
    ]
    for hdf5_file in hdf5_files:
        assert uffd_compare(hdf5_file) is True


# FIXME: This FTP server seems to have disappeared. Replace with something else?
hdf5_list = [
    (
        "ftp://ftp.hdfgroup.uiuc.edu/pub/outgoing/hdf_files/hdf5/samples/convert",
        "C1979091.h5",
        "HDF4_PALGROUP/HDF4_PALETTE_2",
        7488,
        -1,
    ),
    (
        "ftp://ftp.hdfgroup.uiuc.edu/pub/outgoing/hdf_files/hdf5/samples/convert",
        "C1979091.h5",
        "Raster_Image_#0",
        3661,
        -1,
    ),
    (
        "ftp://ftp.hdfgroup.uiuc.edu/pub/outgoing/hdf_files/hdf5/geospatial/DEM",
        "half_moon_bay.grid",
        "HDFEOS/GRIDS/DEMGRID/Data_Fields/Elevation",
        30863,
        -1,
    ),
]


@pytest.mark.parametrize(
    "downloadURL,fileName,subdatasetname,checksum,download_size",
    hdf5_list,
    ids=['HDF5:"' + item[1] + '"://' + item[2] for item in hdf5_list],
)
def test_hdf5(downloadURL, fileName, subdatasetname, checksum, download_size):
    gdaltest.download_or_skip(downloadURL + "/" + fileName, fileName, download_size)

    ds = gdal.Open('HDF5:"tmp/cache/' + fileName + '"://' + subdatasetname)

    assert (
        ds.GetRasterBand(1).Checksum() == checksum
    ), "Bad checksum. Expected %d, got %d" % (checksum, ds.GetRasterBand(1).Checksum())


def test_hdf5_dimension_labels_with_null():
    assert gdal.Open("data/hdf5/dimension_labels_with_null.h5")


def test_hdf5_recursive_groups():

    # File generated with
    # import h5py
    # f = h5py.File('hdf5/recursive_groups.h5','w')
    # group = f.create_group("subgroup")
    # group['link_to_root'] = f
    # group['link_to_self'] = group
    # group['soft_link_to_root'] = h5py.SoftLink('/')
    # group['soft_link_to_self'] = h5py.SoftLink('/subgroup')
    # group['soft_link_to_not_existing'] = h5py.SoftLink('/not_existing')
    # group['hard_link_to_root'] = h5py.HardLink('/')
    # group['ext_link_to_self_root'] = h5py.ExternalLink("hdf5/recursive_groups.h5", "/")
    # f.close()

    ds = gdal.Open("data/hdf5/recursive_groups.h5")
    assert ds is not None
    ds.GetSubDatasets()


def test_hdf5_family_driver():

    assert gdal.Open("data/hdf5/test_family_0.h5")


def test_hdf5_single_dim():

    ds = gdal.Open("HDF5:data/netcdf/byte_chunked_multiple.nc://x")
    assert ds
    b = ds.GetRasterBand(1)
    assert b.YSize == 1
    assert b.XSize == 20
    assert b.GetBlockSize() == [20, 1]
    assert b.Checksum() == 231


###############################################################################
# Test opening a file whose HDF5 signature is not at the beginning


def test_hdf5_signature_not_at_beginning():

    filename = "/vsimem/test.h5"
    gdal.FileFromMemBuffer(
        filename, open("data/netcdf/byte_hdf5_starting_at_offset_1024.nc", "rb").read()
    )
    ds = gdal.Open(filename)
    assert ds is not None
    gdal.Unlink(filename)


###############################################################################
# Test RasterIO() optimizations


def test_hdf5_rasterio_optims():

    # Band-interleaved data
    ds = gdal.Open(
        'HDF5:"data/hdf5/dummy_HDFEOS_swath.h5"://HDFEOS/SWATHS/MySwath/Data_Fields/MyDataField'
    )
    expected = array.array("B", [i for i in range(2 * 3 * 4)]).tobytes()
    assert ds.ReadRaster() == expected
    assert (
        ds.GetRasterBand(1).ReadRaster() + ds.GetRasterBand(2).ReadRaster() == expected
    )

    # optimization through intermediate MEMDataset: non natural interleaving
    assert (
        ds.ReadRaster(buf_pixel_space=ds.RasterCount, buf_band_space=1)
        == array.array(
            "B",
            [
                0,
                12,
                1,
                13,
                2,
                14,
                3,
                15,
                4,
                16,
                5,
                17,
                6,
                18,
                7,
                19,
                8,
                20,
                9,
                21,
                10,
                22,
                11,
                23,
            ],
        ).tobytes()
    )

    # optimization through intermediate MEMDataset: non natural data type
    expected = array.array("H", [i for i in range(2 * 3 * 4)]).tobytes()
    assert ds.ReadRaster(buf_type=gdal.GDT_UInt16) == expected
    assert (
        ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt16)
        + ds.GetRasterBand(2).ReadRaster(buf_type=gdal.GDT_UInt16)
        == expected
    )

    # non-optimized: out of order bands
    assert (
        ds.ReadRaster(band_list=[2, 1])
        == ds.GetRasterBand(2).ReadRaster() + ds.GetRasterBand(1).ReadRaster()
    )

    # non-optimized: resampling
    assert (
        ds.GetRasterBand(1).ReadRaster(3, 2, 1, 1, buf_xsize=2, buf_ysize=2)
        == b"\x0b" * 4
    )

    # Pixel-interleaved data
    ds = gdal.Open("data/hdf5/dummy_HDFEOS_with_sinu_projection.h5")
    assert (
        ds.ReadRaster(buf_pixel_space=ds.RasterCount, buf_band_space=1)
        == array.array("B", [i for i in range(5 * 4 * 3)]).tobytes()
    )

    # optimization through intermediate MEMDataset: non natural interleaving
    assert ds.ReadRaster() == array.array(
        "B",
        [
            0,
            3,
            6,
            9,
            12,
            15,
            18,
            21,
            24,
            27,
            30,
            33,
            36,
            39,
            42,
            45,
            48,
            51,
            54,
            57,
            1,
            4,
            7,
            10,
            13,
            16,
            19,
            22,
            25,
            28,
            31,
            34,
            37,
            40,
            43,
            46,
            49,
            52,
            55,
            58,
            2,
            5,
            8,
            11,
            14,
            17,
            20,
            23,
            26,
            29,
            32,
            35,
            38,
            41,
            44,
            47,
            50,
            53,
            56,
            59,
        ],
    )


###############################################################################
# Test opening a HDF5EOS grid file


def test_hdf5_eos_grid_sinu_projection():

    if False:

        import h5py
        import numpy as np

        # Minimum version of https://github.com/OSGeo/gdal/issues/7117
        f = h5py.File("data/hdf5/dummy_HDFEOS_with_sinu_projection.h5", "w")
        HDFEOS_INFORMATION = f.create_group("HDFEOS INFORMATION")
        # Hint from https://forum.hdfgroup.org/t/nullpad-nullterm-strings/9107
        # to use the low-level API to be able to generate NULLTERM strings
        # without padding bytes
        HDFEOSVersion_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        HDFEOSVersion_type.set_size(32)
        HDFEOSVersion_type.set_strpad(h5py.h5t.STR_NULLTERM)
        # HDFEOS_INFORMATION.attrs.create("HDFEOSVersion", "HDFEOS_5.1.15", dtype=HDFEOSVersion_type)
        HDFEOSVersion_attr = h5py.h5a.create(
            HDFEOS_INFORMATION.id,
            "HDFEOSVersion".encode("ASCII"),
            HDFEOSVersion_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        HDFEOSVersion_value = "HDFEOS_5.1.15".encode("ASCII")
        HDFEOSVersion_value = np.frombuffer(
            HDFEOSVersion_value, dtype="|S%d" % len(HDFEOSVersion_value)
        )
        HDFEOSVersion_attr.write(HDFEOSVersion_value)

        StructMetadata_0_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        StructMetadata_0_type.set_size(32000)
        StructMetadata_0_type.set_strpad(h5py.h5t.STR_NULLTERM)
        StructMetadata_0 = """GROUP=SwathStructure\nEND_GROUP=SwathStructure\nGROUP=GridStructure\n\tGROUP=GRID_1\n\t\tGridName=\"test\"\n\t\tXDim=4\n\t\tYDim=5\n\t\tUpperLeftPointMtrs=(-1111950.519667,5559752.598333)\n\t\tLowerRightMtrs=(0.000000,4447802.078667)\n\t\tProjection=HE5_GCTP_SNSOID\n\t\tProjParams=(6371007.181000,0,0,0,0,0,0,0,0,0,0,0,0)\n\t\tSphereCode=-1\n\t\tGridOrigin=HE5_HDFE_GD_UL\n\t\tGROUP=Dimension\n\t\t\tOBJECT=Dimension_1\n\t\t\t\tDimensionName=\"YDim\"\n\t\t\t\tSize=5\n\t\t\tEND_OBJECT=Dimension_1\n\t\t\tOBJECT=Dimension_2\n\t\t\t\tDimensionName=\"XDim\"\n\t\t\t\tSize=4\n\t\t\tEND_OBJECT=Dimension_2\n\t\t\tOBJECT=Dimension_3\n\t\t\t\tDimensionName=\"Num_Parameters\"\n\t\t\t\tSize=3\n\t\t\tEND_OBJECT=Dimension_3\n\t\tEND_GROUP=Dimension\n\t\tGROUP=DataField\n\t\t\tOBJECT=DataField_1\n\t\t\t\tDataFieldName=\"test\"\n\t\t\t\tDataType=H5T_NATIVE_UCHAR\n\t\t\t\tDimList=(\"YDim\",\"XDim\",\"Num_Parameters\")\n\t\t\t\tMaxdimList=(\"YDim\",\"XDim\",\"Num_Parameters\")\n\t\t\tEND_OBJECT=DataField_1\n\t\tEND_GROUP=DataField\n\t\tGROUP=MergedFields\n\t\tEND_GROUP=MergedFields\n\tEND_GROUP=GRID_1\nEND_GROUP=GridStructure\nGROUP=PointStructure\nEND_GROUP=PointStructure\nGROUP=ZaStructure\nEND_GROUP=ZaStructure\nEND\n"""
        # HDFEOS_INFORMATION.create_dataset("StructMetadata.0", None, data=StructMetadata_0, dtype=StructMetadata_0_type)
        StructMetadata_0_dataset = h5py.h5d.create(
            HDFEOS_INFORMATION.id,
            "StructMetadata.0".encode("ASCII"),
            StructMetadata_0_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        StructMetadata_0_value = StructMetadata_0.encode("ASCII")
        StructMetadata_0_value = np.frombuffer(
            StructMetadata_0_value, dtype="|S%d" % len(StructMetadata_0_value)
        )
        StructMetadata_0_dataset.write(
            h5py.h5s.create(h5py.h5s.SCALAR),
            h5py.h5s.create(h5py.h5s.SCALAR),
            StructMetadata_0_value,
        )

        HDFEOS = f.create_group("HDFEOS")
        ADDITIONAL = HDFEOS.create_group("ADDITIONAL")
        ADDITIONAL.create_group("FILE_ATTRIBUTES")
        GRIDS = HDFEOS.create_group("GRIDS")
        test = GRIDS.create_group("test")
        DataFields = test.create_group("Data Fields")
        ds = DataFields.create_dataset("test", (5, 4, 3), dtype="B")
        ds[...] = np.array([i for i in range(5 * 4 * 3)]).reshape(ds.shape)
        f.close()

    ds = gdal.Open("data/hdf5/dummy_HDFEOS_with_sinu_projection.h5")
    assert ds
    assert ds.RasterXSize == 4
    assert ds.RasterYSize == 5
    assert ds.RasterCount == 3
    assert ds.GetGeoTransform() == pytest.approx(
        (
            -1111950.519667,
            277987.62991675,
            0.0,
            5559752.598333,
            0.0,
            -222390.10393320007,
        )
    )
    assert (
        ds.GetSpatialRef().ExportToProj4()
        == "+proj=sinu +lon_0=0 +x_0=0 +y_0=0 +R=6371007.181 +units=m +no_defs"
    )
    import struct

    assert list(
        struct.unpack(
            "B" * (5 * 4 * 3), ds.ReadRaster(buf_pixel_space=3, buf_band_space=1)
        )
    ) == [i for i in range(5 * 4 * 3)]
    ds = None


###############################################################################
# Test opening a HDF5EOS grid file


def test_hdf5_eos_grid_utm_projection():

    if False:

        import h5py
        import numpy as np

        f = h5py.File("data/hdf5/dummy_HDFEOS_with_utm_projection.h5", "w")
        HDFEOS_INFORMATION = f.create_group("HDFEOS INFORMATION")
        # Hint from https://forum.hdfgroup.org/t/nullpad-nullterm-strings/9107
        # to use the low-level API to be able to generate NULLTERM strings
        # without padding bytes
        HDFEOSVersion_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        HDFEOSVersion_type.set_size(32)
        HDFEOSVersion_type.set_strpad(h5py.h5t.STR_NULLTERM)
        # HDFEOS_INFORMATION.attrs.create("HDFEOSVersion", "HDFEOS_5.1.15", dtype=HDFEOSVersion_type)
        HDFEOSVersion_attr = h5py.h5a.create(
            HDFEOS_INFORMATION.id,
            "HDFEOSVersion".encode("ASCII"),
            HDFEOSVersion_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        HDFEOSVersion_value = "HDFEOS_5.1.15".encode("ASCII")
        HDFEOSVersion_value = np.frombuffer(
            HDFEOSVersion_value, dtype="|S%d" % len(HDFEOSVersion_value)
        )
        HDFEOSVersion_attr.write(HDFEOSVersion_value)

        StructMetadata_0_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        StructMetadata_0_type.set_size(32000)
        StructMetadata_0_type.set_strpad(h5py.h5t.STR_NULLTERM)
        StructMetadata_0 = """GROUP=SwathStructure\nEND_GROUP=SwathStructure\nGROUP=GridStructure\n\tGROUP=GRID_1\n\t\tGridName=\"test\"\n\t\tXDim=20\n\t\tYDim=20\n\t\tUpperLeftPointMtrs=(440720.000, 3751320.000)\n\t\tLowerRightMtrs=(441920.000, 3750120.000)\n\t\tProjection=HE5_GCTP_UTM\n\t\tZoneCode=11\n\t\tSphereCode=12\n\t\tGridOrigin=HE5_HDFE_GD_UL\n\t\tGROUP=Dimension\n\t\t\tOBJECT=Dimension_1\n\t\t\t\tDimensionName=\"YDim\"\n\t\t\t\tSize=20\n\t\t\tEND_OBJECT=Dimension_1\n\t\t\tOBJECT=Dimension_2\n\t\t\t\tDimensionName=\"XDim\"\n\t\t\t\tSize=20\n\t\t\tEND_OBJECT=Dimension_2\n\t\tEND_GROUP=Dimension\n\t\tGROUP=DataField\n\t\t\tOBJECT=DataField_1\n\t\t\t\tDataFieldName=\"test\"\n\t\t\t\tDataType=H5T_NATIVE_UCHAR\n\t\t\t\tDimList=(\"YDim\",\"XDim\")\n\t\t\t\tMaxdimList=(\"YDim\",\"XDim\")\n\t\t\tEND_OBJECT=DataField_1\n\t\tEND_GROUP=DataField\n\t\tGROUP=MergedFields\n\t\tEND_GROUP=MergedFields\n\tEND_GROUP=GRID_1\nEND_GROUP=GridStructure\nGROUP=PointStructure\nEND_GROUP=PointStructure\nGROUP=ZaStructure\nEND_GROUP=ZaStructure\nEND\n"""
        # HDFEOS_INFORMATION.create_dataset("StructMetadata.0", None, data=StructMetadata_0, dtype=StructMetadata_0_type)
        StructMetadata_0_dataset = h5py.h5d.create(
            HDFEOS_INFORMATION.id,
            "StructMetadata.0".encode("ASCII"),
            StructMetadata_0_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        StructMetadata_0_value = StructMetadata_0.encode("ASCII")
        StructMetadata_0_value = np.frombuffer(
            StructMetadata_0_value, dtype="|S%d" % len(StructMetadata_0_value)
        )
        StructMetadata_0_dataset.write(
            h5py.h5s.create(h5py.h5s.SCALAR),
            h5py.h5s.create(h5py.h5s.SCALAR),
            StructMetadata_0_value,
        )

        HDFEOS = f.create_group("HDFEOS")
        ADDITIONAL = HDFEOS.create_group("ADDITIONAL")
        ADDITIONAL.create_group("FILE_ATTRIBUTES")
        GRIDS = HDFEOS.create_group("GRIDS")
        test = GRIDS.create_group("test")
        DataFields = test.create_group("Data Fields")
        ds = DataFields.create_dataset("test", (20, 20), dtype="B")
        ds[...] = np.array([i for i in range(20 * 20)]).reshape(ds.shape)
        f.close()

    ds = gdal.Open("data/hdf5/dummy_HDFEOS_with_utm_projection.h5")
    assert ds
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetGeoTransform() == pytest.approx((440720, 60, 0, 3751320, 0, -60))
    assert ds.GetSpatialRef().GetUTMZone() == 11
    # WGS 84
    assert ds.GetSpatialRef().GetSemiMajor() == 6378137
    assert ds.GetSpatialRef().GetInvFlattening() == 298.257223563
    ds = None


###############################################################################
# Test opening a HDF5EOS grid file


def test_hdf5_eos_grid_utm_projection_empty_GROUP_dimension():

    # Test support for products following
    # AMSR-E/AMSR2 Unified L3 Daily 12.5 km Brightness Temperatures,
    # Sea Ice Concentration, Motion & Snow Depth Polar Grids
    # (https://nsidc.org/sites/default/files/au_si12-v001-userguide_1.pdf)
    # such as
    # https://n5eil01u.ecs.nsidc.org/AMSA/AU_SI12.001/2012.07.02/AMSR_U2_L3_SeaIce12km_B04_20120702.he5

    if False:

        import h5py
        import numpy as np

        f = h5py.File(
            "data/hdf5/dummy_HDFEOS_with_utm_projection_empty_GROUP_dimension.h5", "w"
        )
        HDFEOS_INFORMATION = f.create_group("HDFEOS INFORMATION")
        # Hint from https://forum.hdfgroup.org/t/nullpad-nullterm-strings/9107
        # to use the low-level API to be able to generate NULLTERM strings
        # without padding bytes
        HDFEOSVersion_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        HDFEOSVersion_type.set_size(32)
        HDFEOSVersion_type.set_strpad(h5py.h5t.STR_NULLTERM)
        # HDFEOS_INFORMATION.attrs.create("HDFEOSVersion", "HDFEOS_5.1.15", dtype=HDFEOSVersion_type)
        HDFEOSVersion_attr = h5py.h5a.create(
            HDFEOS_INFORMATION.id,
            "HDFEOSVersion".encode("ASCII"),
            HDFEOSVersion_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        HDFEOSVersion_value = "HDFEOS_5.1.15".encode("ASCII")
        HDFEOSVersion_value = np.frombuffer(
            HDFEOSVersion_value, dtype="|S%d" % len(HDFEOSVersion_value)
        )
        HDFEOSVersion_attr.write(HDFEOSVersion_value)

        StructMetadata_0_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        StructMetadata_0_type.set_size(32000)
        StructMetadata_0_type.set_strpad(h5py.h5t.STR_NULLTERM)
        StructMetadata_0 = """GROUP=SwathStructure\nEND_GROUP=SwathStructure\nGROUP=GridStructure\n\tGROUP=GRID_1\n\t\tGridName=\"test\"\n\t\tXDim=10\n\t\tYDim=20\n\t\tUpperLeftPointMtrs=(440720.000, 3751320.000)\n\t\tLowerRightMtrs=(441920.000, 3750120.000)\n\t\tProjection=HE5_GCTP_UTM\n\t\tZoneCode=11\n\t\tSphereCode=12\n\t\tGridOrigin=HE5_HDFE_GD_UL\n\t\tGROUP=Dimension\n\t\tEND_GROUP=Dimension\n\t\tGROUP=DataField\n\t\t\tOBJECT=DataField_1\n\t\t\t\tDataFieldName=\"test\"\n\t\t\t\tDataType=H5T_NATIVE_UCHAR\n\t\t\t\tDimList=(\"YDim\",\"XDim\")\n\t\t\t\tMaxdimList=(\"YDim\",\"XDim\")\n\t\t\tEND_OBJECT=DataField_1\n\t\tEND_GROUP=DataField\n\t\tGROUP=MergedFields\n\t\tEND_GROUP=MergedFields\n\tEND_GROUP=GRID_1\nEND_GROUP=GridStructure\nGROUP=PointStructure\nEND_GROUP=PointStructure\nGROUP=ZaStructure\nEND_GROUP=ZaStructure\nEND\n"""
        # HDFEOS_INFORMATION.create_dataset("StructMetadata.0", None, data=StructMetadata_0, dtype=StructMetadata_0_type)
        StructMetadata_0_dataset = h5py.h5d.create(
            HDFEOS_INFORMATION.id,
            "StructMetadata.0".encode("ASCII"),
            StructMetadata_0_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        StructMetadata_0_value = StructMetadata_0.encode("ASCII")
        StructMetadata_0_value = np.frombuffer(
            StructMetadata_0_value, dtype="|S%d" % len(StructMetadata_0_value)
        )
        StructMetadata_0_dataset.write(
            h5py.h5s.create(h5py.h5s.SCALAR),
            h5py.h5s.create(h5py.h5s.SCALAR),
            StructMetadata_0_value,
        )

        HDFEOS = f.create_group("HDFEOS")
        ADDITIONAL = HDFEOS.create_group("ADDITIONAL")
        ADDITIONAL.create_group("FILE_ATTRIBUTES")
        GRIDS = HDFEOS.create_group("GRIDS")
        test = GRIDS.create_group("test")
        DataFields = test.create_group("Data Fields")
        ds = DataFields.create_dataset("test", (20, 10), dtype="B")
        ds[...] = np.array([i for i in range(20 * 10)]).reshape(ds.shape)
        f.close()

    ds = gdal.Open(
        "data/hdf5/dummy_HDFEOS_with_utm_projection_empty_GROUP_dimension.h5"
    )
    assert ds
    assert ds.RasterXSize == 10
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetGeoTransform() == pytest.approx((440720, 120, 0, 3751320, 0, -60))
    assert ds.GetSpatialRef().GetUTMZone() == 11
    # WGS 84
    assert ds.GetSpatialRef().GetSemiMajor() == 6378137
    assert ds.GetSpatialRef().GetInvFlattening() == 298.257223563
    ds = None


###############################################################################
# Test opening a HDF5EOS grid file


def test_hdf5_eos_grid_geo_projection():

    if False:

        import h5py
        import numpy as np

        f = h5py.File("data/hdf5/dummy_HDFEOS_with_geo_projection.h5", "w")
        HDFEOS_INFORMATION = f.create_group("HDFEOS INFORMATION")
        # Hint from https://forum.hdfgroup.org/t/nullpad-nullterm-strings/9107
        # to use the low-level API to be able to generate NULLTERM strings
        # without padding bytes
        HDFEOSVersion_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        HDFEOSVersion_type.set_size(32)
        HDFEOSVersion_type.set_strpad(h5py.h5t.STR_NULLTERM)
        # HDFEOS_INFORMATION.attrs.create("HDFEOSVersion", "HDFEOS_5.1.15", dtype=HDFEOSVersion_type)
        HDFEOSVersion_attr = h5py.h5a.create(
            HDFEOS_INFORMATION.id,
            "HDFEOSVersion".encode("ASCII"),
            HDFEOSVersion_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        HDFEOSVersion_value = "HDFEOS_5.1.15".encode("ASCII")
        HDFEOSVersion_value = np.frombuffer(
            HDFEOSVersion_value, dtype="|S%d" % len(HDFEOSVersion_value)
        )
        HDFEOSVersion_attr.write(HDFEOSVersion_value)

        StructMetadata_0_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        StructMetadata_0_type.set_size(32000)
        StructMetadata_0_type.set_strpad(h5py.h5t.STR_NULLTERM)
        StructMetadata_0 = """GROUP=SwathStructure\nEND_GROUP=SwathStructure\nGROUP=GridStructure\n\tGROUP=GRID_1\n\t\tGridName=\"test\"\n\t\tXDim=20\n\t\tYDim=20\n\t\tUpperLeftPointMtrs=(-117038028.21, 33054002.17)\n\t\tLowerRightMtrs=(-117037041.20, 33053023.45)\n\t\tProjection=HE5_GCTP_GEO\n\t\tSphereCode=12\n\t\tGridOrigin=HE5_HDFE_GD_UL\n\t\tGROUP=Dimension\n\t\t\tOBJECT=Dimension_1\n\t\t\t\tDimensionName=\"YDim\"\n\t\t\t\tSize=20\n\t\t\tEND_OBJECT=Dimension_1\n\t\t\tOBJECT=Dimension_2\n\t\t\t\tDimensionName=\"XDim\"\n\t\t\t\tSize=20\n\t\t\tEND_OBJECT=Dimension_2\n\t\tEND_GROUP=Dimension\n\t\tGROUP=DataField\n\t\t\tOBJECT=DataField_1\n\t\t\t\tDataFieldName=\"test\"\n\t\t\t\tDataType=H5T_NATIVE_UCHAR\n\t\t\t\tDimList=(\"YDim\",\"XDim\")\n\t\t\t\tMaxdimList=(\"YDim\",\"XDim\")\n\t\t\tEND_OBJECT=DataField_1\n\t\tEND_GROUP=DataField\n\t\tGROUP=MergedFields\n\t\tEND_GROUP=MergedFields\n\tEND_GROUP=GRID_1\nEND_GROUP=GridStructure\nGROUP=PointStructure\nEND_GROUP=PointStructure\nGROUP=ZaStructure\nEND_GROUP=ZaStructure\nEND\n"""
        # HDFEOS_INFORMATION.create_dataset("StructMetadata.0", None, data=StructMetadata_0, dtype=StructMetadata_0_type)
        StructMetadata_0_dataset = h5py.h5d.create(
            HDFEOS_INFORMATION.id,
            "StructMetadata.0".encode("ASCII"),
            StructMetadata_0_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        StructMetadata_0_value = StructMetadata_0.encode("ASCII")
        StructMetadata_0_value = np.frombuffer(
            StructMetadata_0_value, dtype="|S%d" % len(StructMetadata_0_value)
        )
        StructMetadata_0_dataset.write(
            h5py.h5s.create(h5py.h5s.SCALAR),
            h5py.h5s.create(h5py.h5s.SCALAR),
            StructMetadata_0_value,
        )

        HDFEOS = f.create_group("HDFEOS")
        ADDITIONAL = HDFEOS.create_group("ADDITIONAL")
        ADDITIONAL.create_group("FILE_ATTRIBUTES")
        GRIDS = HDFEOS.create_group("GRIDS")
        test = GRIDS.create_group("test")
        DataFields = test.create_group("Data Fields")
        ds = DataFields.create_dataset("test", (20, 20), dtype="B")
        ds[...] = np.array([i for i in range(20 * 20)]).reshape(ds.shape)
        f.close()

    ds = gdal.Open("data/hdf5/dummy_HDFEOS_with_geo_projection.h5")
    assert ds
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetGeoTransform() == pytest.approx(
        (
            -117.64116944444262,
            0.0006529166665345087,
            0.0,
            33.900602777778275,
            0.0,
            -0.00053777777781292,
        )
    )
    assert ds.GetSpatialRef().IsGeographic()
    # WGS 84
    assert ds.GetSpatialRef().GetSemiMajor() == 6378137
    assert ds.GetSpatialRef().GetInvFlattening() == 298.257223563
    ds = None


###############################################################################
# Test opening a HDF5EOS swatch file (the Swath.h5 file generated by
# 'make check' on the hdfeos5 library)


def test_hdf5_eos_swath_with_explicit_dimension_map():

    # Using the DimensionMap (the data field has more samples than the geo fields)
    ds = gdal.Open(
        'HDF5:"data/hdf5/hdfeos_sample_swath.h5"://HDFEOS/SWATHS/Swath1/Data_Fields/Spectra'
    )
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 40
    assert ds.RasterCount == 15
    assert ds.GetMetadata("GEOLOCATION") == {
        "LINE_OFFSET": "0",
        "LINE_STEP": "2",
        "PIXEL_OFFSET": "1",
        "PIXEL_STEP": "2",
        "SRS": 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]',
        "X_BAND": "1",
        "X_DATASET": 'HDF5:"data/hdf5/hdfeos_sample_swath.h5"://HDFEOS/SWATHS/Swath1/Geolocation_Fields/Longitude',
        "Y_BAND": "1",
        "Y_DATASET": 'HDF5:"data/hdf5/hdfeos_sample_swath.h5"://HDFEOS/SWATHS/Swath1/Geolocation_Fields/Latitude',
        "GEOREFERENCING_CONVENTION": "PIXEL_CENTER",
    }
    assert gdal.Open(ds.GetMetadataItem("X_DATASET", "GEOLOCATION")) is not None
    assert gdal.Open(ds.GetMetadataItem("Y_DATASET", "GEOLOCATION")) is not None

    # Not using the DimensionMap (the data field uses the same dimensions as the geo fields)
    ds = gdal.Open(
        'HDF5:"data/hdf5/hdfeos_sample_swath.h5"://HDFEOS/SWATHS/Swath1/Data_Fields/Temperature'
    )
    assert ds.RasterXSize == 10
    assert ds.RasterYSize == 20
    assert ds.RasterCount == 1
    assert ds.GetMetadata("GEOLOCATION") == {
        "LINE_OFFSET": "0",
        "LINE_STEP": "1",
        "PIXEL_OFFSET": "0",
        "PIXEL_STEP": "1",
        "SRS": 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]',
        "X_BAND": "1",
        "X_DATASET": 'HDF5:"data/hdf5/hdfeos_sample_swath.h5"://HDFEOS/SWATHS/Swath1/Geolocation_Fields/Longitude',
        "Y_BAND": "1",
        "Y_DATASET": 'HDF5:"data/hdf5/hdfeos_sample_swath.h5"://HDFEOS/SWATHS/Swath1/Geolocation_Fields/Latitude',
        "GEOREFERENCING_CONVENTION": "PIXEL_CENTER",
    }
    assert gdal.Open(ds.GetMetadataItem("X_DATASET", "GEOLOCATION")) is not None
    assert gdal.Open(ds.GetMetadataItem("Y_DATASET", "GEOLOCATION")) is not None
    ds = None


###############################################################################
# Test opening a HDF5EOS swath file


def test_hdf5_eos_swath_no_explicit_dimension_map():

    if False:

        import h5py
        import numpy as np

        f = h5py.File("data/hdf5/dummy_HDFEOS_swath.h5", "w")
        HDFEOS_INFORMATION = f.create_group("HDFEOS INFORMATION")
        # Hint from https://forum.hdfgroup.org/t/nullpad-nullterm-strings/9107
        # to use the low-level API to be able to generate NULLTERM strings
        # without padding bytes
        HDFEOSVersion_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        HDFEOSVersion_type.set_size(32)
        HDFEOSVersion_type.set_strpad(h5py.h5t.STR_NULLTERM)
        # HDFEOS_INFORMATION.attrs.create("HDFEOSVersion", "HDFEOS_5.1.15", dtype=HDFEOSVersion_type)
        HDFEOSVersion_attr = h5py.h5a.create(
            HDFEOS_INFORMATION.id,
            "HDFEOSVersion".encode("ASCII"),
            HDFEOSVersion_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        HDFEOSVersion_value = "HDFEOS_5.1.15".encode("ASCII")
        HDFEOSVersion_value = np.frombuffer(
            HDFEOSVersion_value, dtype="|S%d" % len(HDFEOSVersion_value)
        )
        HDFEOSVersion_attr.write(HDFEOSVersion_value)

        StructMetadata_0_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        StructMetadata_0_type.set_size(32000)
        StructMetadata_0_type.set_strpad(h5py.h5t.STR_NULLTERM)
        StructMetadata_0 = """GROUP=SwathStructure
    GROUP=SWATH_1
        SwathName="MySwath"
        GROUP=Dimension
            OBJECT=Dimension_1
                DimensionName="Band"
                Size=2
            END_OBJECT=Dimension_1
            OBJECT=Dimension_2
                DimensionName="AlongTrack"
                Size=3
            END_OBJECT=Dimension_2
            OBJECT=Dimension_3
                DimensionName="CrossTrack"
                Size=4
            END_OBJECT=Dimension_3
        END_GROUP=Dimension
        GROUP=DimensionMap
        END_GROUP=DimensionMap
        GROUP=IndexDimensionMap
        END_GROUP=IndexDimensionMap
        GROUP=GeoField
            OBJECT=GeoField_1
                GeoFieldName="Latitude"
                DataType=H5T_NATIVE_FLOAT
                DimList=("AlongTrack","CrossTrack")
                MaxdimList=("AlongTrack","CrossTrack")
            END_OBJECT=GeoField_1
            OBJECT=GeoField_2
                GeoFieldName="Longitude"
                DataType=H5T_NATIVE_FLOAT
                DimList=("AlongTrack","CrossTrack")
                MaxdimList=("AlongTrack","CrossTrack")
            END_OBJECT=GeoField_2
            OBJECT=GeoField_3
                GeoFieldName="Time"
                DataType=H5T_NATIVE_FLOAT
                DimList=("AlongTrack")
                MaxdimList=("AlongTrack")
            END_OBJECT=GeoField_3
        END_GROUP=GeoField
        GROUP=DataField
            OBJECT=DataField_1
                DataFieldName="MyDataField"
                DataType=H5T_NATIVE_FLOAT
                DimList=("Band","AlongTrack","CrossTrack")
                MaxdimList=("Band","AlongTrack","CrossTrack")
            END_OBJECT=DataField_1
        END_GROUP=DataField
        GROUP=ProfileField
        END_GROUP=ProfileField
        GROUP=MergedFields
        END_GROUP=MergedFields
    END_GROUP=SWATH_1
END_GROUP=SwathStructure
GROUP=GridStructure
END_GROUP=GridStructure
END
"""
        StructMetadata_0_dataset = h5py.h5d.create(
            HDFEOS_INFORMATION.id,
            "StructMetadata.0".encode("ASCII"),
            StructMetadata_0_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        StructMetadata_0_value = StructMetadata_0.encode("ASCII")
        StructMetadata_0_value = np.frombuffer(
            StructMetadata_0_value, dtype="|S%d" % len(StructMetadata_0_value)
        )
        StructMetadata_0_dataset.write(
            h5py.h5s.create(h5py.h5s.SCALAR),
            h5py.h5s.create(h5py.h5s.SCALAR),
            StructMetadata_0_value,
        )

        HDFEOS = f.create_group("HDFEOS")
        ADDITIONAL = HDFEOS.create_group("ADDITIONAL")
        ADDITIONAL.create_group("FILE_ATTRIBUTES")
        SWATHS = HDFEOS.create_group("SWATHS")
        MySwath = SWATHS.create_group("MySwath")
        DataFields = MySwath.create_group("Data Fields")
        ds = DataFields.create_dataset("MyDataField", (2, 3, 4), dtype="B")
        ds[...] = np.array([i for i in range(2 * 3 * 4)]).reshape(ds.shape)
        GeoLocationFields = MySwath.create_group("Geolocation Fields")
        ds = GeoLocationFields.create_dataset("Longitude", (3, 4), dtype="f")
        ds[...] = np.array([i for i in range(3 * 4)]).reshape(ds.shape)
        ds = GeoLocationFields.create_dataset("Latitude", (3, 4), dtype="f")
        ds[...] = np.array([i for i in range(3 * 4)]).reshape(ds.shape)
        f.close()

    ds = gdal.Open("data/hdf5/dummy_HDFEOS_swath.h5")
    subds = ds.GetSubDatasets()
    assert (
        subds[0][0]
        == 'HDF5:"data/hdf5/dummy_HDFEOS_swath.h5"://HDFEOS/SWATHS/MySwath/Data_Fields/MyDataField'
    )
    assert subds[0][1].startswith(
        "[(Band=2)x(AlongTrack=3)x(CrossTrack=4)] //HDFEOS/SWATHS/MySwath/Data_Fields/MyDataField (8-bit"
    )

    ds = gdal.Open(subds[0][0])
    assert ds.RasterXSize == 4
    assert ds.RasterYSize == 3
    assert ds.RasterCount == 2
    assert len(ds.GetGCPs()) == 0
    assert ds.GetMetadata("GEOLOCATION") == {
        "GEOREFERENCING_CONVENTION": "PIXEL_CENTER",
        "LINE_OFFSET": "0",
        "LINE_STEP": "1",
        "PIXEL_OFFSET": "0",
        "PIXEL_STEP": "1",
        "SRS": 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]',
        "X_BAND": "1",
        "X_DATASET": 'HDF5:"data/hdf5/dummy_HDFEOS_swath.h5"://HDFEOS/SWATHS/MySwath/Geolocation_Fields/Longitude',
        "Y_BAND": "1",
        "Y_DATASET": 'HDF5:"data/hdf5/dummy_HDFEOS_swath.h5"://HDFEOS/SWATHS/MySwath/Geolocation_Fields/Latitude',
    }
    assert gdal.Open(ds.GetMetadataItem("X_DATASET", "GEOLOCATION")) is not None
    assert gdal.Open(ds.GetMetadataItem("Y_DATASET", "GEOLOCATION")) is not None
    ds = None

    ds = gdal.Open(
        'HDF5:"data/hdf5/dummy_HDFEOS_swath.h5"://HDFEOS/SWATHS/MySwath/Geolocation_Fields/Longitude'
    )
    assert len(ds.GetGCPs()) == 0
    assert ds.GetMetadata("GEOLOCATION") is not None


###############################################################################
# Test opening a HDF5EOS swath file with a .aux.xml


def test_hdf5_eos_swath_with_aux_xml():

    ds = gdal.Open(
        'HDF5:"data/hdf5/dummy_HDFEOS_swath_with_aux_xml_with_geolocation.h5"://HDFEOS/SWATHS/MySwath/Data_Fields/MyDataField'
    )
    assert (
        "data/hdf5/dummy_HDFEOS_swath_with_aux_xml_with_geolocation.h5.aux.xml"
        in ds.GetFileList()
    )
    assert (
        b"/i/do/not/exist/dummy_HDFEOS_swath_with_aux_xml_with_geolocation.h5"
        in open(
            "data/hdf5/dummy_HDFEOS_swath_with_aux_xml_with_geolocation.h5.aux.xml",
            "rb",
        ).read()
    )
    assert (
        ds.GetMetadataItem("X_DATASET", "GEOLOCATION")
        == 'HDF5:"data/hdf5/dummy_HDFEOS_swath_with_aux_xml_with_geolocation.h5"://HDFEOS/SWATHS/MySwath/Geolocation_Fields/Longitude'
    )
    assert (
        ds.GetMetadataItem("Y_DATASET", "GEOLOCATION")
        == 'HDF5:"data/hdf5/dummy_HDFEOS_swath_with_aux_xml_with_geolocation.h5"://HDFEOS/SWATHS/MySwath/Geolocation_Fields/Latitude'
    )


###############################################################################
# Test opening a file with band specific attributes


def test_hdf5_band_specific_attribute():

    if False:

        import h5py

        f = h5py.File("data/hdf5/fwhm.h5", "w")
        ds = f.create_dataset("MyDataField", (2, 3, 4), dtype="B")
        ds.attrs["fwhm"] = [0.01, 0.02]
        ds.attrs["fwhm_units"] = "Micrometers"
        ds.attrs["bad_band_list"] = [0, 1]
        ds.attrs["center_wavelengths"] = [300, 400]
        ds.attrs["my_coefficients"] = [1, 2]
        f.close()

    ds = gdal.Open("data/hdf5/fwhm.h5")
    assert ds.RasterXSize == 4
    assert ds.RasterYSize == 3
    assert ds.RasterCount == 2
    assert ds.GetRasterBand(1).GetMetadata_Dict() == {
        "fwhm": "0.01",
        "fwhm_units": "Micrometers",
        "bad_band": "0",
        "center_wavelength": "300",
        "my_coefficient": "1",
    }
    assert ds.GetRasterBand(2).GetMetadata_Dict() == {
        "fwhm": "0.02",
        "fwhm_units": "Micrometers",
        "bad_band": "1",
        "center_wavelength": "400",
        "my_coefficient": "2",
    }
    ds = None


###############################################################################
# Test gdal subdataset informational functions


@pytest.mark.parametrize(
    "filename,path_component",
    (
        (
            'HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex',
            "OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5",
        ),
        (
            "HDF5:OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex",
            "OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5",
        ),
        (
            r'HDF5:"C:\OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex',
            r"C:\OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5",
        ),
        (
            r'HDF5:"C:/OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex',
            r"C:/OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5",
        ),
        (
            r'HDF5:"/vsicurl/http://www.my.com/OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex',
            r"/vsicurl/http://www.my.com/OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5",
        ),
        (
            r"HDF5:a://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex",
            r"a",
        ),
        (
            r"HDF5:a:/my/path://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex",
            r"a:/my/path",
        ),
        ("", ""),
    ),
)
def test_gdal_subdataset_get_filename(filename, path_component):

    info = gdal.GetSubdatasetInfo(filename)
    if filename == "":
        assert info is None
    else:
        assert info.GetPathComponent() == path_component
        assert (
            info.GetSubdatasetComponent()
            == "//HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex"
        )


@pytest.mark.parametrize(
    "filename",
    (
        'HDF5:"OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex',
        r'HDF5:"C:\OMI-Aura_L2-OMTO3_2005m0113t0224-o02648_v002-2005m0625t035355.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex',
        "",
    ),
)
def test_gdal_subdataset_modify_filename(filename):

    info = gdal.GetSubdatasetInfo(filename)
    if filename == "":
        assert info is None
    else:
        assert (
            info.ModifyPathComponent('"/path/to.he5"')
            == 'HDF5:"/path/to.he5"://HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex'
        )


@pytest.mark.parametrize(
    "bogus",
    (
        "HDF5:a:c",
        "HDF5:a",
        "HDF5:",
    ),
)
def test_gdal_subdataset_bogus(bogus):
    """Test it doesn't crash"""

    gdal.GetSubdatasetInfo(bogus)


###############################################################################
# Test opening a chunked HDF5EOS swath file


def test_hdf5_eos_swath_chunking_optimization():

    if False:

        import h5py
        import numpy as np

        f = h5py.File("data/hdf5/dummy_HDFEOS_swath_chunked.h5", "w")
        HDFEOS_INFORMATION = f.create_group("HDFEOS INFORMATION")
        # Hint from https://forum.hdfgroup.org/t/nullpad-nullterm-strings/9107
        # to use the low-level API to be able to generate NULLTERM strings
        # without padding bytes
        HDFEOSVersion_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        HDFEOSVersion_type.set_size(32)
        HDFEOSVersion_type.set_strpad(h5py.h5t.STR_NULLTERM)
        # HDFEOS_INFORMATION.attrs.create("HDFEOSVersion", "HDFEOS_5.1.15", dtype=HDFEOSVersion_type)
        HDFEOSVersion_attr = h5py.h5a.create(
            HDFEOS_INFORMATION.id,
            "HDFEOSVersion".encode("ASCII"),
            HDFEOSVersion_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        HDFEOSVersion_value = "HDFEOS_5.1.15".encode("ASCII")
        HDFEOSVersion_value = np.frombuffer(
            HDFEOSVersion_value, dtype="|S%d" % len(HDFEOSVersion_value)
        )
        HDFEOSVersion_attr.write(HDFEOSVersion_value)

        StructMetadata_0_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        StructMetadata_0_type.set_size(32000)
        StructMetadata_0_type.set_strpad(h5py.h5t.STR_NULLTERM)
        StructMetadata_0 = """GROUP=SwathStructure
    GROUP=SWATH_1
        SwathName="MySwath"
        GROUP=Dimension
            OBJECT=Dimension_1
                DimensionName="Band"
                Size=20
            END_OBJECT=Dimension_1
            OBJECT=Dimension_2
                DimensionName="AlongTrack"
                Size=30
            END_OBJECT=Dimension_2
            OBJECT=Dimension_3
                DimensionName="CrossTrack"
                Size=40
            END_OBJECT=Dimension_3
        END_GROUP=Dimension
        GROUP=DimensionMap
        END_GROUP=DimensionMap
        GROUP=IndexDimensionMap
        END_GROUP=IndexDimensionMap
        GROUP=GeoField
            OBJECT=GeoField_1
                GeoFieldName="Latitude"
                DataType=H5T_NATIVE_FLOAT
                DimList=("AlongTrack","CrossTrack")
                MaxdimList=("AlongTrack","CrossTrack")
            END_OBJECT=GeoField_1
            OBJECT=GeoField_2
                GeoFieldName="Longitude"
                DataType=H5T_NATIVE_FLOAT
                DimList=("AlongTrack","CrossTrack")
                MaxdimList=("AlongTrack","CrossTrack")
            END_OBJECT=GeoField_2
            OBJECT=GeoField_3
                GeoFieldName="Time"
                DataType=H5T_NATIVE_FLOAT
                DimList=("AlongTrack")
                MaxdimList=("AlongTrack")
            END_OBJECT=GeoField_3
        END_GROUP=GeoField
        GROUP=DataField
            OBJECT=DataField_1
                DataFieldName="MyDataField"
                DataType=H5T_NATIVE_FLOAT
                DimList=("Band","AlongTrack","CrossTrack")
                MaxdimList=("Band","AlongTrack","CrossTrack")
            END_OBJECT=DataField_1
        END_GROUP=DataField
        GROUP=ProfileField
        END_GROUP=ProfileField
        GROUP=MergedFields
        END_GROUP=MergedFields
    END_GROUP=SWATH_1
END_GROUP=SwathStructure
GROUP=GridStructure
END_GROUP=GridStructure
END
"""
        StructMetadata_0_dataset = h5py.h5d.create(
            HDFEOS_INFORMATION.id,
            "StructMetadata.0".encode("ASCII"),
            StructMetadata_0_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        StructMetadata_0_value = StructMetadata_0.encode("ASCII")
        StructMetadata_0_value = np.frombuffer(
            StructMetadata_0_value, dtype="|S%d" % len(StructMetadata_0_value)
        )
        StructMetadata_0_dataset.write(
            h5py.h5s.create(h5py.h5s.SCALAR),
            h5py.h5s.create(h5py.h5s.SCALAR),
            StructMetadata_0_value,
        )

        HDFEOS = f.create_group("HDFEOS")
        ADDITIONAL = HDFEOS.create_group("ADDITIONAL")
        ADDITIONAL.create_group("FILE_ATTRIBUTES")
        SWATHS = HDFEOS.create_group("SWATHS")
        MySwath = SWATHS.create_group("MySwath")
        DataFields = MySwath.create_group("Data Fields")
        ds = DataFields.create_dataset(
            "MyDataField", (20, 30, 40), chunks=(3, 4, 6), dtype="f", compression="gzip"
        )
        ds[...] = np.array([i for i in range(20 * 30 * 40)]).reshape(ds.shape)
        GeoLocationFields = MySwath.create_group("Geolocation Fields")
        ds = GeoLocationFields.create_dataset("Longitude", (20, 30), dtype="f")
        ds[...] = np.array([i for i in range(20 * 30)]).reshape(ds.shape)
        ds = GeoLocationFields.create_dataset("Latitude", (20, 30), dtype="f")
        ds[...] = np.array([i for i in range(20 * 30)]).reshape(ds.shape)
        f.close()

    ds = gdal.Open(
        'HDF5:"data/hdf5/dummy_HDFEOS_swath_chunked.h5"://HDFEOS/SWATHS/MySwath/Data_Fields/MyDataField'
    )
    mem_ds = gdal.Translate("", ds, format="MEM")

    ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
    assert ds.GetMetadataItem("WholeBandChunkOptim", "__DEBUG__") == "DISABLED"

    ds.GetRasterBand(1).ReadRaster(0, 0, ds.RasterXSize, 1)
    assert (
        ds.GetMetadataItem("WholeBandChunkOptim", "__DEBUG__")
        == "DETECTION_IN_PROGRESS"
    )
    ds.GetRasterBand(1).ReadRaster(0, 2, ds.RasterXSize, 1)
    assert ds.GetMetadataItem("WholeBandChunkOptim", "__DEBUG__") == "DISABLED"

    ds.GetRasterBand(1).ReadRaster(0, 0, ds.RasterXSize, 1)
    assert (
        ds.GetMetadataItem("WholeBandChunkOptim", "__DEBUG__")
        == "DETECTION_IN_PROGRESS"
    )
    ds.GetRasterBand(2).ReadRaster(0, 0, ds.RasterXSize, 1)
    assert ds.GetMetadataItem("WholeBandChunkOptim", "__DEBUG__") == "DISABLED"

    ds = gdal.Open(
        'HDF5:"data/hdf5/dummy_HDFEOS_swath_chunked.h5"://HDFEOS/SWATHS/MySwath/Data_Fields/MyDataField'
    )
    assert (
        ds.GetMetadataItem("WholeBandChunkOptim", "__DEBUG__")
        == "DETECTION_IN_PROGRESS"
    )
    for i in range(ds.RasterCount):
        assert (
            ds.GetRasterBand(i + 1).Checksum() == mem_ds.GetRasterBand(i + 1).Checksum()
        )
        assert ds.GetMetadataItem("WholeBandChunkOptim", "__DEBUG__") == (
            "DETECTION_IN_PROGRESS" if i == 0 else "ENABLED"
        )
    for i in range(ds.RasterCount):
        assert (
            ds.GetRasterBand(i + 1).ReadRaster()
            == mem_ds.GetRasterBand(i + 1).ReadRaster()
        )
        assert ds.GetMetadataItem("WholeBandChunkOptim", "__DEBUG__") == "ENABLED"
        assert ds.GetRasterBand(i + 1).ReadRaster(1, 2, 3, 4) == mem_ds.GetRasterBand(
            i + 1
        ).ReadRaster(1, 2, 3, 4)
        assert ds.GetMetadataItem("WholeBandChunkOptim", "__DEBUG__") == "ENABLED"

    blockxsize, blockysize = ds.GetRasterBand(1).GetBlockSize()
    assert ds.GetRasterBand(1).ReadBlock(1, 2) == mem_ds.GetRasterBand(1).ReadRaster(
        1 * blockxsize, 2 * blockysize, blockxsize, blockysize
    )


###############################################################################
# Test GetNoDataValue(), GetOffset(), GetScale()


def test_hdf5_read_netcdf_nodata_scale_offset():

    ds = gdal.Open("data/hdf5/scale_offset.h5")
    band = ds.GetRasterBand(1)
    assert band.GetNoDataValue() == pytest.approx(9.96921e36, rel=1e-7)
    assert band.GetOffset() == 1.5
    assert band.GetScale() == 0.01


###############################################################################
# Test force opening a netCDF file with HDF5 driver


def test_hdf5_force_opening_netcdf_file():

    ds = gdal.OpenEx("data/netcdf/trmm-nc4.nc", allowed_drivers=["HDF5"])
    assert ds.GetDriver().GetDescription() == "HDF5Image"

    ds = gdal.OpenEx(
        "data/netcdf/byte_hdf5_starting_at_offset_1024.nc", allowed_drivers=["HDF5"]
    )
    assert ds.GetDriver().GetDescription() == "HDF5Image"


###############################################################################
# Test force opening, but provided file is still not recognized (for good reasons)


def test_hdf5_force_opening_no_match():

    drv = gdal.IdentifyDriverEx("data/byte.tif", allowed_drivers=["HDF5"])
    assert drv is None


###############################################################################
@gdaltest.enable_exceptions()
def test_hdf5_open_larger_than_INT_MAX_pixels():

    with gdal.OpenEx(
        "data/bag/larger_than_INT_MAX_pixels.bag", allowed_drivers=["HDF5"]
    ) as ds:
        assert len(ds.GetSubDatasets()) == 0

    with pytest.raises(Exception, match="At least one dimension size exceeds INT_MAX"):
        gdal.Open('HDF5:"data/bag/larger_than_INT_MAX_pixels.bag"://BAG_root/elevation')


###############################################################################
# Test opening a HDF5EOS grid file


def test_hdf5_eos_grid_VIIRS_Grid_IMG_2D_issue_12941():

    if False:

        import h5py
        import numpy as np

        f = h5py.File("data/hdf5/dummy_HDFEOS_IIRS_Grid_IMG_2D_issue_1294.h5", "w")
        HDFEOS_INFORMATION = f.create_group("HDFEOS INFORMATION")
        # Hint from https://forum.hdfgroup.org/t/nullpad-nullterm-strings/9107
        # to use the low-level API to be able to generate NULLTERM strings
        # without padding bytes
        HDFEOSVersion_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        HDFEOSVersion_type.set_size(32)
        HDFEOSVersion_type.set_strpad(h5py.h5t.STR_NULLTERM)
        # HDFEOS_INFORMATION.attrs.create("HDFEOSVersion", "HDFEOS_5.1.15", dtype=HDFEOSVersion_type)
        HDFEOSVersion_attr = h5py.h5a.create(
            HDFEOS_INFORMATION.id,
            "HDFEOSVersion".encode("ASCII"),
            HDFEOSVersion_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        HDFEOSVersion_value = "HDFEOS_5.1.15".encode("ASCII")
        HDFEOSVersion_value = np.frombuffer(
            HDFEOSVersion_value, dtype="|S%d" % len(HDFEOSVersion_value)
        )
        HDFEOSVersion_attr.write(HDFEOSVersion_value)

        StructMetadata_0_type = h5py.h5t.TypeID.copy(h5py.h5t.C_S1)
        StructMetadata_0_type.set_size(32000)
        StructMetadata_0_type.set_strpad(h5py.h5t.STR_NULLTERM)
        StructMetadata_0 = """GROUP=SwathStructure\nEND_GROUP=SwathStructure\nGROUP=GridStructure\n\tGROUP=GRID_1\n\t\tGridName=\"test\"\n\t\tXDim=4\n\t\tYDim=5\n\t\tUpperLeftPointMtrs=(-1111950.519667,5559752.598333)\n\t\tLowerRightMtrs=(0.000000,4447802.078667)\n\t\tProjection=HE5_GCTP_SNSOID\n\t\tProjParams=(6371007.181000,0,0,0,0,0,0,0,0,0,0,0,0)\n\t\tSphereCode=-1\n\t\tGROUP=Dimension\n\t\t\tOBJECT=Dimension_1\n\t\t\t\tDimensionName=\"unrelated\"\n\t\t\t\tSize=15\n\t\t\tEND_OBJECT=Dimension_1\n\t\tEND_GROUP=Dimension\n\t\tGROUP=DataField\n\t\t\tOBJECT=DataField_1\n\t\t\t\tDataFieldName=\"test\"\n\t\t\t\tDataType=H5T_NATIVE_UCHAR\n\t\t\t\tDimList=(\"YDim\",\"XDim\")\n\t\t\t\tMaxdimList=(\"YDim\",\"XDim\")\n\t\t\tEND_OBJECT=DataField_1\n\t\tEND_GROUP=DataField\n\t\tGROUP=MergedFields\n\t\tEND_GROUP=MergedFields\n\tEND_GROUP=GRID_1\nEND_GROUP=GridStructure\nGROUP=PointStructure\nEND_GROUP=PointStructure\nGROUP=ZaStructure\nEND_GROUP=ZaStructure\nEND\n"""
        # HDFEOS_INFORMATION.create_dataset("StructMetadata.0", None, data=StructMetadata_0, dtype=StructMetadata_0_type)
        StructMetadata_0_dataset = h5py.h5d.create(
            HDFEOS_INFORMATION.id,
            "StructMetadata.0".encode("ASCII"),
            StructMetadata_0_type,
            h5py.h5s.create(h5py.h5s.SCALAR),
        )
        StructMetadata_0_value = StructMetadata_0.encode("ASCII")
        StructMetadata_0_value = np.frombuffer(
            StructMetadata_0_value, dtype="|S%d" % len(StructMetadata_0_value)
        )
        StructMetadata_0_dataset.write(
            h5py.h5s.create(h5py.h5s.SCALAR),
            h5py.h5s.create(h5py.h5s.SCALAR),
            StructMetadata_0_value,
        )

        HDFEOS = f.create_group("HDFEOS")
        ADDITIONAL = HDFEOS.create_group("ADDITIONAL")
        ADDITIONAL.create_group("FILE_ATTRIBUTES")
        GRIDS = HDFEOS.create_group("GRIDS")
        test = GRIDS.create_group("test")

        DataFields = test.create_group("Data Fields")
        ds = DataFields.create_dataset("test", (5, 4), dtype="B")
        ds[...] = np.array([i for i in range(5 * 4)]).reshape(ds.shape)

        ds = test.create_dataset("XDim", (4), dtype="d")
        ds.attrs["CLASS"] = "DIMENSION_SCALE"
        ds[...] = np.array([10, 20, 30, 40]).reshape(ds.shape)

        ds = test.create_dataset("YDim", (5), dtype="d")
        ds.attrs["CLASS"] = "DIMENSION_SCALE"
        ds[...] = np.array([500, 400, 300, 200, 100]).reshape(ds.shape)

        f.close()

    ds = gdal.Open("data/hdf5/dummy_HDFEOS_IIRS_Grid_IMG_2D_issue_1294.h5")
    assert ds
    assert ds.RasterXSize == 4
    assert ds.RasterYSize == 5
    print(ds.GetGeoTransform())
    assert ds.GetGeoTransform() == pytest.approx(
        (
            -1111950.519667,
            277987.62991675,
            0.0,
            5559752.598333,
            0.0,
            -222390.10393320007,
        )
    )
