#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for all datatypes from a MiraMon file.
# Author:   Abel Pau <a.pau@creaf.cat>
#
###############################################################################
# Copyright (c) 2025, Abel Pau <a.pau@creaf.cat>
#
# SPDX-License-Identifier: MIT
###############################################################################


import math
import os
import struct

import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("MiraMonRaster")

gdal_to_struct = {
    gdal.GDT_UInt8: ("B", 1),
    gdal.GDT_UInt16: ("H", 2),
    gdal.GDT_Int16: ("h", 2),
    gdal.GDT_UInt32: ("I", 4),
    gdal.GDT_Int32: ("i", 4),
    gdal.GDT_Float32: ("f", 4),
    gdal.GDT_Float64: ("d", 8),
}


###### Testing IMG/REL normal files
def check_raster(ds, band_idx, expected, checksum, exp_min, exp_max, exp_gt):
    band = ds.GetRasterBand(band_idx)

    # Dataset geotransform
    gt = ds.GetGeoTransform()
    assert gt is not None, "GeoTransform not found"
    if gt is not None and exp_gt is not None:
        for i in range(6):
            assert (
                abs(gt[i] - exp_gt[i]) < 1e-6
            ), f"GeoTransform element {i} mismatch: got {gt[i]}, expected {exp_gt[i]}"

    # Bands
    assert band is not None, f"Error opening band {band_idx}"
    rchecksum = band.Checksum()

    assert rchecksum == checksum, f"Unexpected checksum: {rchecksum}"

    # size of the raster
    xsize = band.XSize
    ysize = band.YSize
    dtype = band.DataType
    assert dtype in gdal_to_struct, f"Unsupported GDAL data type: {dtype}"

    # min and max values
    min_val = band.GetMinimum()
    max_val = band.GetMaximum()

    assert min_val == exp_min
    assert max_val == exp_max

    # Coherence between ColorTable i PaletteInterpretation
    ct = band.GetColorTable()
    cint = band.GetColorInterpretation()
    if ct is None:
        assert (
            cint == gdal.GCI_GrayIndex
        ), f"Band {band_idx} should have GrayIndex as ColorInterpretation"
    else:
        assert (
            cint == gdal.GCI_PaletteIndex
        ), f"Band {band_idx} should have PaletteIndex as ColorInterpretation"

    # Reading content
    fmt, size = gdal_to_struct[dtype]
    buf = band.ReadRaster(0, 0, xsize, ysize, buf_type=dtype)
    assert buf is not None, "Could not read raster data"

    # unpack and assert values
    count = xsize * ysize
    values = struct.unpack(f"{count}{fmt}", buf)

    for i, exp in enumerate(expected):
        assert (
            values[i] == exp
        ), f"Unexpected pixel value at index {i}: got {values[i]}, expected {exp}"


expected_gt = (516792.0, 2.0, 0.0, 4638260.0, 0.0, -2.0)
expected_gt2 = (0.0, 1.0, 0.0, 8.0, 0.0, -1)
init_list = [
    (
        "data/miramon/normal/byte_2x3_6_categs.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/byte_2x3_6_categsI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/integer_2x3_6_categs.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/integer_2x3_6_categsI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/uinteger_2x3_6_categs.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/uinteger_2x3_6_categsI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/long_2x3_6_categs.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/long_2x3_6_categsI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/real_2x3_6_categs.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/real_2x3_6_categsI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/double_2x3_6_categs.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/double_2x3_6_categsI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/byte_2x3_6_categs_RLE.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/byte_2x3_6_categs_RLEI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/byte_2x3_6_categs_RLE_no_ind.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/byte_2x3_6_categs_RLE_no_indI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/integer_2x3_6_categs_RLE.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/integer_2x3_6_categs_RLEI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/uinteger_2x3_6_categs_RLE.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/uinteger_2x3_6_categs_RLEI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/long_2x3_6_categs_RLE.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/long_2x3_6_categs_RLEI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/real_2x3_6_categs_RLE.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/real_2x3_6_categs_RLEI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/double_2x3_6_categs_RLE.img",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/double_2x3_6_categs_RLEI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        5,
        expected_gt,
    ),
    (
        "data/miramon/normal/chess_bit.img",
        1,
        [0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0],
        32,
        0,
        1,
        expected_gt2,
    ),
    (
        "data/miramon/normal/chess_bitI.rel",
        1,
        [0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0],
        32,
        0,
        1,
        expected_gt2,
    ),
    (
        "data/miramon/all_nodata/nodataI.rel",
        1,
        [0, 0, 0, 0, 0, 0],
        0,
        None,
        None,
        None,
    ),
]


@pytest.mark.parametrize(
    "filename,band_idx,expected,checksum,exp_min,exp_max,exp_gt",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
def test_miramon_test_basic_raster(
    filename, band_idx, expected, checksum, exp_min, exp_max, exp_gt
):
    # ds = gdal.Open(filename)
    ds = gdal.OpenEx(filename, allowed_drivers=["MiraMonRaster"])
    assert ds is not None, "Could not open the file"
    check_raster(ds, band_idx, expected, checksum, exp_min, exp_max, exp_gt)


###### Testing IMG/REL files with controlled errors
@pytest.mark.parametrize(
    "name,message_substring",
    [
        (
            "data/miramon/several_errors/alone_rel.rel",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/several_errors/alone_IrelI.rel",
            "must have VersMetaDades>=4",
        ),
        (
            "data/miramon/several_errors/empy_img.img",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/several_errors/empy_relI.rel",
            "must be REL4",
        ),
        (
            "data/miramon/several_errors/no_assoc_img.rel",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/several_errors/no_assoc_rel.img",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/several_errors/no_colI.rel",
            "No number of columns documented",
        ),
        (
            "data/miramon/several_errors/no_rowI.rel",
            "No number of rows documented",
        ),
        (
            "data/miramon/several_errors/no_zero_col_rowI.rel",
            "(nWidth <= 0 || nHeight <= 0)",
        ),
        (
            "data/miramon/several_errors/no_bandsI.rel",
            "ATTRIBUTE_DATA-IndexsNomsCamps section-key should exist",
        ),
        (
            "data/miramon/several_errors/no_bands2I.rel",
            "it has zero usable bands",
        ),
        (
            "data/miramon/several_errors/no_bands3I.rel",
            "ATTRIBUTE_DATA-IndexsNomsCamps section-key should exist",
        ),
        (
            "data/miramon/several_errors/no_typeI.rel",
            "MiraMonRaster: no nDataType documented",
        ),
        (
            "data/miramon/several_errors/wrong_typeI.rel",
            "MiraMonRaster: data type unhandled",
        ),
        (
            "data/miramon/several_errors/wrong_band_nameI.rel",
            "Failed to open MiraMon band file",
        ),
    ],
)
def test_miramon_test_fails(name, message_substring):
    with pytest.raises(Exception) as excinfo:
        gdal.OpenEx(
            name,
            gdal.OF_RASTER,
        )
    assert message_substring in str(excinfo.value)


###### Testing subdatasets
init_list_subdatasets = [
    (
        "data/miramon/multiband/byte_2x3_6_multibandI.rel",
        5,
        0,
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        None,
        0,
        5,
    ),
    (
        "data/miramon/multiband/byte_2x3_6_categs.img",
        5,
        0,
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        None,
        0,
        5,
    ),
    (
        "data/miramon/multiband/byte_2x3_6_multibandI.rel",
        5,
        1,
        1,
        [0, 1, 2, 3, 4, 255],
        10,
        255,
        0,
        4,
    ),
    (
        "data/miramon/multiband/byte_2x3_0_to_4_categs_NoData_255.img",
        5,
        1,
        1,
        [0, 1, 2, 3, 4, 255],
        10,
        255,
        0,
        4,
    ),
    (
        "data/miramon/multiband/byte_2x3_6_multibandI.rel",
        5,
        2,
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        1,
        5,
    ),
    (
        "data/miramon/multiband/byte_2x3_1_to_5_categs_NoData_0.img",
        5,
        2,
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
        1,
        5,
    ),
]


@pytest.mark.parametrize(
    "filename,n_exp_sds,idx_sds,idx_bnd,expected,checksum,expected_nodata,exp_min,exp_max",
    init_list_subdatasets,
    ids=[tup[0].split("/")[-1].split(".")[0] for tup in init_list_subdatasets],
)
def test_miramon_subdatasets_detection(
    filename,
    n_exp_sds,
    idx_sds,
    idx_bnd,
    expected,
    checksum,
    expected_nodata,
    exp_min,
    exp_max,
):
    ds = gdal.OpenEx(filename, allowed_drivers=["MiraMonRaster"])
    assert ds is not None, f"Could not open file: {filename}"

    subdatasets = ds.GetSubDatasets()
    assert subdatasets is not None, "GetSubDatasets() returned None"
    assert (
        len(subdatasets) == n_exp_sds
    ), f"Expected {n_exp_sds} subdatasets, got {len(subdatasets)}"

    # Let's open every one of them
    subdataset_name, desc = subdatasets[idx_sds]
    subds = gdal.OpenEx(subdataset_name, allowed_drivers=["MiraMonRaster"])
    assert subds is not None, f"Could not open subdataset: {subdataset_name}"
    band = subds.GetRasterBand(idx_bnd)
    assert band is not None, "Could not get band from subdataset"
    checksum = band.Checksum()
    assert checksum >= 0, "Invalid checksum from subdataset"
    nodata = band.GetNoDataValue()
    if nodata is not None:
        assert (
            nodata == expected_nodata
        ), f"Unexpected nodata value : got {nodata}, expected {expected_nodata}"

    check_raster(subds, idx_bnd, expected, checksum, exp_min, exp_max, None)


###### Testing number of subdatasets for a multiband file
###### that has different characteristics.
# Tested characteristics:
# data type
# compression
# extension (minx and maxx)
# extension amplidude
# Categorical vs continuous
# nodata value
# existence of nodata value
# existence of color table


def test_miramon_subdatasets_number():
    ds = gdal.OpenEx(
        "data/miramon/subdatasets/byteI.rel", allowed_drivers=["MiraMonRaster"]
    )
    assert ds is not None, "Could not open file: data/miramon/subdatasets/byteI.rel"

    subdatasets = ds.GetSubDatasets()
    assert subdatasets is not None, "GetSubDatasets() returned None"
    assert len(subdatasets) == 10, f"Expected 10 subdatasets, got {len(subdatasets)}"


###### Testing color table
init_list_color_tables = [
    (
        "data/miramon/normal/byte_2x3_6_categsI.rel",
        1,  # band index
        {  # color table
            0: (0, 0, 255, 255),
            1: (0, 255, 255, 255),
            2: (0, 255, 0, 255),
            3: (255, 255, 0, 255),
            4: (255, 0, 0, 255),
            5: (255, 0, 255, 255),
        },
        "25831",  # reference system
    ),
    (
        "data/miramon/palettes/Constant/byte_2x3_6_categsI.rel",
        1,  # band index
        {  # color table
            0: (255, 0, 255, 255),
            1: (255, 0, 255, 255),
            2: (255, 0, 255, 255),
            3: (255, 0, 255, 255),
            4: (255, 0, 255, 255),
            5: (0, 0, 0, 0),
        },
        "25831",  # reference system
    ),
    (
        "data/miramon/palettes/Categorical/Automatic/byte_2x3_6_categsI.rel",
        1,  # band index
        None,
        "25831",
    ),
    (
        "data/miramon/all_nodata/nodataI.rel",
        1,  # band index
        None,
        None,
    ),
    (
        "data/miramon/several_errors/WrongPaletteI.rel",
        1,  # band index
        None,
        "25831",
    ),
    (
        "data/miramon/several_errors/WrongPalette2I.rel",
        1,  # band index
        None,
        "25831",
    ),
    (
        "data/miramon/several_errors/WrongPalette3I.rel",
        1,  # band index
        None,
        "25831",
    ),
    (
        "data/miramon/several_errors/NonExistantPaletteI.rel",
        1,  # band index
        None,
        "25831",
    ),
    (
        "data/miramon/several_errors/EmptyPaletteI.rel",
        1,  # band index
        None,
        "25831",
    ),
    (
        "data/miramon/palettes/Continous/ColorTable/uinteger_with_nodataI.rel",
        1,  # band index
        None,
        None,
    ),
    (
        "data/miramon/palettes/Continous/ColorTable/double_with_nodataI.rel",
        1,  # band index
        None,
        None,
    ),
    (
        "data/miramon/palettes/Categorical/Assigned/byte_2x3_6_categsI.rel",
        1,  # band index
        {
            0: (0, 0, 125, 255),
            1: (0, 134, 255, 255),
            2: (0, 255, 0, 255),
            3: (255, 255, 78, 255),
            4: (255, 0, 0, 255),
            5: (255, 0, 133, 255),
        },
        "25831",
    ),
    (
        "data/miramon/palettes/Categorical/Assigned/real_2x3_6_categsI.rel",
        1,  # band index
        {
            0: (0, 0, 125, 255),
            1: (0, 134, 255, 255),
            2: (0, 255, 0, 255),
            3: (255, 255, 78, 255),
            4: (255, 0, 0, 255),
            5: (255, 0, 133, 255),
        },
        "25831",
    ),
    (
        "data/miramon/palettes/Categorical/Assignedp25/byte_2x3_6_categsI.rel",
        1,  # band index
        {
            0: (0, 0, 0, 255),
            1: (0, 97, 0, 255),
            2: (0, 162, 0, 255),
            3: (0, 255, 0, 255),
            4: (255, 255, 0, 255),
            5: (255, 210, 0, 255),
            15: (255, 178, 255, 255),
        },
        "25831",
    ),
    (
        "data/miramon/palettes/Categorical/AssignedPAL/byte_2x3_6_categsI.rel",
        1,  # band index
        {
            0: (0, 0, 0, 255),
            1: (0, 24, 0, 255),
            2: (0, 40, 0, 255),
            3: (0, 63, 0, 255),
            4: (63, 63, 0, 255),
            5: (63, 52, 0, 255),
            15: (63, 44, 63, 255),
        },
        "25831",
    ),
    (
        "data/miramon/palettes/Categorical/Assignedp65/byte_2x3_6_categsI.rel",
        1,  # band index
        {
            0: (0, 0, 0, 255),
            1: (0, 24, 0, 255),
            2: (0, 40, 0, 255),
            3: (0, 63, 0, 255),
            4: (63, 63, 0, 255),
            5: (63, 52, 0, 255),
            15: (63, 44, 63, 255),
        },
        "25831",
    ),
    (
        "data/miramon/palettes/Categorical/ThematicNoDataBeg/MUCSC_2002_30_m_v_6_retI.rel",
        1,  # band index
        {  # some colours are tested
            0: (0, 0, 0, 0),
            1: (212, 247, 255, 255),
            2: (153, 247, 245, 255),
            8: (255, 255, 201, 255),
            9: (184, 201, 189, 255),
            14: (145, 108, 0, 255),
            15: (83, 166, 0, 255),
            16: (149, 206, 0, 255),
            20: (65, 206, 0, 255),
            21: (128, 0, 128, 255),
            24: (201, 232, 163, 255),
        },
        "25831",
    ),
]


@pytest.mark.parametrize(
    "filename,idx_bnd,expected_ct,exp_epsg",
    init_list_color_tables,
    # ids=[tup[0].split("/")[-1].split(".")[0] for tup in init_list_color_tables],
    ids=[
        os.path.join(
            os.path.basename(os.path.dirname(tup[0])),
            os.path.splitext(os.path.basename(tup[0]))[0],
        )
        for tup in init_list_color_tables
    ],
)
def test_miramon_epsg_and_color_table(filename, idx_bnd, expected_ct, exp_epsg):
    ds = gdal.OpenEx(filename, allowed_drivers=["MiraMonRaster"])
    assert ds is not None, f"Could not open file: {filename}"

    # Comparing reference system
    if exp_epsg is not None:
        srs = ds.GetSpatialRef()
        if (
            srs is not None
        ):  # in Fedora it returns None (but it's the only system it does)
            epsg_code = srs.GetAuthorityCode("PROJCS") or srs.GetAuthorityCode("GEOGCS")
            assert (
                epsg_code == exp_epsg
            ), f"incorrect EPSG: {epsg_code}, waited {exp_epsg}"

    # Comparing color table
    band = ds.GetRasterBand(idx_bnd)
    assert band is not None, f"Could not get band {idx_bnd} from file"

    if expected_ct == None:
        try:
            ct = band.GetColorTable()
        except RuntimeError:
            pass
    else:
        ct = band.GetColorTable()
        assert ct is not None, "No color table found on band"
        assert (
            band.GetColorInterpretation() == gdal.GCI_PaletteIndex
        ), f"band {idx_bnd} should have PaletteIndex as Color Interpretation"
        for index, expected_color in expected_ct.items():
            entry = ct.GetColorEntry(index)
            assert (
                entry is not None
            ), f"Color entry for index {index} is missing in color table"
            assert (
                tuple(entry) == expected_color
            ), f"Color entry for index {index} does not match: got {entry}, expected {expected_color}"


###### Testing attribute table
init_list_attribute_tables = [
    (
        "data/miramon/palettes/Continous/DBF_nodata_end/double_with_nodataI.rel",
        1,  # band index
        {
            (0, "MIN"): 1.7e308,
            (0, "MAX"): 1.7e308,
            (0, "Red"): 204,
            (0, "Green"): 217,
            (0, "Blue"): 249,
            (2, "MIN"): -2130706431.0078125,
            (2, "MAX"): -2113929215.015625,
            (2, "Red"): 144,
            (2, "Green"): 178,
            (2, "Blue"): 109,
            (52, "MIN"): -1291845631.398438,
            (52, "MAX"): -1275068415.40625,
            (52, "Red"): 192,
            (52, "Green"): 171,
            (52, "Blue"): 89,
            (255, "MIN"): 2113929215.015625,
            (255, "MAX"): 2147483647,
            (255, "Red"): 164,
            (255, "Green"): 106,
            (255, "Blue"): 0,
            (256, "MIN"): 2147483647,
            (256, "MAX"): 2147483647,
            (256, "Red"): 164,
            (256, "Green"): 106,
            (256, "Blue"): 0,
        },
    ),
    (
        "data/miramon/palettes/Continous/ColorTable/uinteger_with_nodataI.rel",
        1,  # band index
        {
            (0, "MIN"): 65535,
            (0, "MAX"): 65535,
            (0, "Red"): 204,
            (0, "Green"): 217,
            (0, "Blue"): 249,
            (1, "MIN"): 0,
            (1, "MAX"): 256,
            (1, "Red"): 149,
            (1, "Green"): 186,
            (1, "Blue"): 116,
            (52, "MIN"): 13056,
            (52, "MAX"): 13312,
            (52, "Red"): 192,
            (52, "Green"): 171,
            (52, "Blue"): 89,
            (255, "MIN"): 65023,
            (255, "MAX"): 65534,
            (255, "Red"): 164,
            (255, "Green"): 106,
            (255, "Blue"): 0,
            (256, "MIN"): 65534,
            (256, "MAX"): 65534,
            (256, "Red"): 164,
            (256, "Green"): 106,
            (256, "Blue"): 0,
        },
    ),
    (
        "data/miramon/palettes/Categorical/ThematicNoDataBeg/MUCSC_2002_30_m_v_6_retI.rel",
        1,  # band index
        {
            (1, "CODI_USCOB"): 1,
            (1, "DESC_USCOB"): "Aigües marines",
            (1, "CAMPEXTRA1"): "extra1",
            (1, "CAMPEXTRA2"): 2,
            (1, "CAMPEXTRA3"): "20250110",
            (1, "CAMPEXTRA4"): "T",
            (2, "CODI_USCOB"): 2,
            (2, "DESC_USCOB"): "Aigües continentals",
            (2, "CAMPEXTRA1"): "extra11",
            (2, "CAMPEXTRA2"): 3,
            (2, "CAMPEXTRA3"): "20250710",
            (2, "CAMPEXTRA4"): "F",
        },
    ),
    (
        "data/miramon/palettes/Categorical/ThematicNoDataEnd/MUCSC_2002_30_m_v_6_retI.rel",
        1,  # band index
        {
            (1, "CODI_USCOB"): 1,
            (1, "DESC_USCOB"): "Aigües marines",
            (1, "CAMPEXTRA1"): "extra1",
            (1, "CAMPEXTRA2"): 2,
            (1, "CAMPEXTRA3"): "20250110",
            (1, "CAMPEXTRA4"): "T",
            (2, "CODI_USCOB"): 2,
            (2, "DESC_USCOB"): "Aigües continentals",
            (2, "CAMPEXTRA1"): "extra11",
            (2, "CAMPEXTRA2"): 3,
            (2, "CAMPEXTRA3"): "20250710",
            (2, "CAMPEXTRA4"): "F",
        },
    ),
    (
        "data/miramon/palettes/Categorical/ThematicNoREL/MUCSC_2002_30_m_v_6_retI.rel",
        1,  # band index
        {
            (1, "CODI_USCOB"): 1,
            (1, "DESC_USCOB"): "Aigües marines",
            (1, "CAMPEXTRA1"): "extra1",
            (1, "CAMPEXTRA2"): 2,
            (1, "CAMPEXTRA3"): "20250110",
            (1, "CAMPEXTRA4"): "T",
            (2, "CODI_USCOB"): 2,
            (2, "DESC_USCOB"): "Aigües continentals",
            (2, "CAMPEXTRA1"): "extra11",
            (2, "CAMPEXTRA2"): 3,
            (2, "CAMPEXTRA3"): "20250710",
            (2, "CAMPEXTRA4"): "F",
        },
    ),
    (
        "data/miramon/palettes/Categorical/ThematicNoDataMiddle/MUCSC_2002_30_m_v_6_retI.rel",
        1,  # band index
        {
            (1, "CODI_USCOB"): 1,
            (1, "DESC_USCOB"): "Aigües marines",
            (1, "CAMPEXTRA1"): "extra1",
            (1, "CAMPEXTRA2"): 2,
            (1, "CAMPEXTRA3"): "20250110",
            (1, "CAMPEXTRA4"): "T",
            (2, "CODI_USCOB"): 2,
            (2, "DESC_USCOB"): "Aigües continentals",
            (2, "CAMPEXTRA1"): "extra11",
            (2, "CAMPEXTRA2"): 3,
            (2, "CAMPEXTRA3"): "20250710",
            (2, "CAMPEXTRA4"): "F",
        },
    ),
    (
        "data/miramon/palettes/Categorical/ThematicLessColors/MUCSC_2002_30_m_v_6_retI.rel",
        1,  # band index
        {
            (1, "CODI_USCOB"): 1,
            (1, "DESC_USCOB"): "Aigües marines",
            (1, "CAMPEXTRA1"): "extra1",
            (1, "CAMPEXTRA2"): 2,
            (1, "CAMPEXTRA3"): "20250110",
            (1, "CAMPEXTRA4"): "T",
            (2, "CODI_USCOB"): 2,
            (2, "DESC_USCOB"): "Aigües continentals",
            (2, "CAMPEXTRA1"): "extra11",
            (2, "CAMPEXTRA2"): 3,
            (2, "CAMPEXTRA3"): "20250710",
            (2, "CAMPEXTRA4"): "F",
        },
    ),
    (
        "data/miramon/all_nodata/nodataI.rel",
        1,  # band index
        None,
    ),
    (
        "data/miramon/palettes/Continous/LinearLogSimbo/LinearSimboWith0I.rel",
        1,  # band index
        {
            (0, "MIN"): 0,
            (0, "MAX"): 1000000,
            (0, "Red"): 81,
            (0, "Green"): 49,
            (0, "Blue"): 0,
            (1, "MIN"): 1000000,
            (1, "MAX"): 2000000,
            (1, "Red"): 129,
            (1, "Green"): 78,
            (1, "Blue"): 0,
            (7, "MIN"): 7000000,
            (7, "MAX"): 8000000,
            (7, "Red"): 255,
            (7, "Green"): 202,
            (7, "Blue"): 0,
            (9, "MIN"): 10000000,
            (9, "MAX"): 10000000,
            (9, "Red"): 255,
            (9, "Green"): 202,
            (9, "Blue"): 0,
        },
    ),
]


@pytest.mark.parametrize(
    "filename,idx_bnd,expected_rat",
    init_list_attribute_tables,
    # ids=[tup[0].split("/")[-1].split(".")[0] for tup in init_list_attribute_tables],
    ids=[
        os.path.join(
            os.path.basename(os.path.dirname(tup[0])),
            os.path.splitext(os.path.basename(tup[0]))[0],
        )
        for tup in init_list_attribute_tables
    ],
)
def test_miramon_default_rat(filename, idx_bnd, expected_rat):
    ds = gdal.OpenEx(filename, allowed_drivers=["MiraMonRaster"])
    assert ds is not None, f"Could not open file: {filename}"

    band = ds.GetRasterBand(idx_bnd)
    assert band is not None, f"Could not get band {idx_bnd} from file"

    rat = band.GetDefaultRAT()
    if expected_rat is not None:
        assert rat is not None, "No Raster Attribute Table (RAT) found on band"

        col_name_to_idx = {rat.GetNameOfCol(i): i for i in range(rat.GetColumnCount())}

        for (row_idx, col_name), expected_val in expected_rat.items():
            assert 0 <= row_idx < rat.GetRowCount(), f"Row {row_idx} out of bounds"
            assert col_name in col_name_to_idx, f"Column '{col_name}' not found in RAT"

            col_idx = col_name_to_idx[col_name]
            gdal_type = rat.GetTypeOfCol(col_idx)

            if gdal_type == gdal.GFT_Integer:
                val = rat.GetValueAsInt(row_idx, col_idx)
            elif gdal_type == gdal.GFT_Real:
                val = rat.GetValueAsDouble(row_idx, col_idx)
            elif gdal_type == gdal.GFT_String:
                val = rat.GetValueAsString(row_idx, col_idx)
            else:
                raise ValueError(
                    f"Unsupported field type {gdal_type} for column '{col_name}'"
                )

            if isinstance(expected_val, float):
                assert math.isclose(val, expected_val, rel_tol=1e-6, abs_tol=1e-12), (
                    f"Float mismatch at row {row_idx}, column '{col_name}': "
                    f"got {val}, expected {expected_val}"
                )
            else:
                assert val == expected_val, (
                    f"Value mismatch at row {row_idx}, column '{col_name}': "
                    f"got {val}, expected {expected_val}"
                )
