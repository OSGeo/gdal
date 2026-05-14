#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic write support for to a MiraMon file.
# Author:   Abel Pau <a.pau@creaf.cat>
#
###############################################################################
# Copyright (c) 2025, Abel Pau <a.pau@creaf.cat>
#
# SPDX-License-Identifier: MIT
###############################################################################

import io
import struct

import pytest

from osgeo import gdal, osr

pytestmark = pytest.mark.require_driver("MiraMonRaster")

# --- Color table on the band ---
colors = [
    (0, 0, 0, 0),  # NoData
    (255, 0, 0, 255),
    (0, 255, 0, 255),
    (0, 0, 255, 255),
    (0, 125, 125, 255),
    (125, 125, 255, 255),
]

# --- RAT on the band ---
classname_list = [
    "Background",
    "Class_1",
    "Class_2",
    "Class_3",
    "Class_4",
    "Class_5",
]
classname_double = [0.1, 1.2, 2.3, 3.4, 4.5, 5.6]

gdal_to_struct = {
    gdal.GDT_UInt8: "B",
    gdal.GDT_Int16: "h",
    gdal.GDT_UInt16: "H",
    gdal.GDT_Int32: "i",
    gdal.GDT_Float32: "f",
    gdal.GDT_Float64: "d",
}

init_type_list = [
    gdal.GDT_UInt8,
    gdal.GDT_Int16,
    gdal.GDT_UInt16,
    gdal.GDT_Int32,
    gdal.GDT_Float32,
    gdal.GDT_Float64,
]


@pytest.mark.parametrize(
    "data_type",
    init_type_list,
)
@pytest.mark.parametrize(
    "compress",
    ["YES", "NO"],
)
@pytest.mark.parametrize(
    "pattern",
    [None, "UserPattern"],
)
@pytest.mark.parametrize(
    "rat_first_col_type", [gdal.GFU_MinMax, gdal.GFU_Min, gdal.GFU_Generic]
)
def test_miramonraster_monoband(
    tmp_path, data_type, compress, pattern, rat_first_col_type
):

    if data_type == gdal.GDT_UInt8:
        use_color_table = "True"
    else:
        use_color_table = "False"

    if data_type == gdal.GDT_UInt8 or data_type == gdal.GDT_UInt16:
        use_rat = "True"
    else:
        use_rat = "False"

    # --- Raster parameters ---
    xsize = 3
    ysize = 2
    geotransform = (100.0, 10.0, 0.0, 200.0, 0.0, -10.0)

    srs = osr.SpatialReference()
    epsg_code = 25831
    srs.ImportFromEPSG(epsg_code)
    wkt = srs.ExportToWkt()

    # --- Create in-memory dataset ---
    mem_driver = gdal.GetDriverByName("MEM")
    src_ds = mem_driver.Create("", xsize, ysize, 1, data_type)

    src_ds.SetGeoTransform(geotransform)
    src_ds.SetProjection(wkt)

    # --- Create deterministic pixel values ---
    # values 0..5
    band_values = list(range(xsize * ysize))

    fmt = gdal_to_struct[data_type]
    band_bytes = struct.pack("<" + fmt * len(band_values), *band_values)

    # --- Write raster data ---
    src_ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        xsize,
        ysize,
        band_bytes,
        buf_xsize=xsize,
        buf_ysize=ysize,
        buf_type=data_type,
    )

    band = src_ds.GetRasterBand(1)
    band.SetNoDataValue(0)
    band.FlushCache()

    if use_color_table == "True":
        ct = gdal.ColorTable()
        for i in range(len(colors)):
            ct.SetColorEntry(i, colors[i])

        band.SetRasterColorTable(ct)
        band.SetRasterColorInterpretation(gdal.GCI_PaletteIndex)

    # --- Raster Attribute Table (RAT) ---
    if use_rat == "True":
        rat = gdal.RasterAttributeTable()
        rat.CreateColumn("Value", gdal.GFT_Integer, rat_first_col_type)
        rat.CreateColumn("ClassName", gdal.GFT_String, gdal.GFU_Name)
        rat.CreateColumn("Real", gdal.GFT_Real, gdal.GFU_Generic)

        rat.SetRowCount(6)

        for i in range(len(classname_list)):
            rat.SetValueAsInt(i, 0, i)
            rat.SetValueAsString(i, 1, classname_list[i])
            rat.SetValueAsDouble(i, 2, classname_double[i])
        band.SetDefaultRAT(rat)

    # --- Write to MiraMonRaster ---
    mm_path = tmp_path / "testI.rel"

    co = [f"COMPRESS={compress}"]
    if pattern is not None:
        co.append(f"PATTERN={pattern}")

    mm_driver = gdal.GetDriverByName("MiraMonRaster")
    assert mm_driver is not None
    mm_ds = mm_driver.CreateCopy(mm_path, src_ds, options=co, callback=None)
    assert mm_ds is not None
    mm_ds = None

    # --- Reopen dataset ---
    dst_ds = gdal.Open(mm_path)
    assert dst_ds is not None
    assert dst_ds.GetDriver().ShortName == "MiraMonRaster"

    subdatasets = dst_ds.GetSubDatasets()
    assert subdatasets is None or len(subdatasets) == 0

    # --- Dataset checks ---
    assert dst_ds.RasterXSize == xsize
    assert dst_ds.RasterYSize == ysize
    assert dst_ds.RasterCount == 1
    assert dst_ds.GetGeoTransform() == geotransform

    # Comparing reference system
    if geotransform is not None:
        srs = dst_ds.GetSpatialRef()
        if (
            srs is not None
        ):  # in Fedora it returns None (but it's the only system it does)
            epsg_code = srs.GetAuthorityCode("PROJCS") or srs.GetAuthorityCode("GEOGCS")
            assert (
                epsg_code == epsg_code
            ), f"incorrect EPSG: {epsg_code}, waited {epsg_code}"

    # --- Pixel data checks ---
    dst_band_bytes = dst_ds.GetRasterBand(1).ReadRaster(
        0, 0, xsize, ysize, buf_xsize=xsize, buf_ysize=ysize, buf_type=data_type
    )

    assert dst_band_bytes == band_bytes

    dst_band = dst_ds.GetRasterBand(1)

    # --- Color table check ---
    if use_color_table == "True":
        dst_ct = dst_band.GetRasterColorTable()
        assert dst_ct is not None
        for i in range(len(colors)):
            assert dst_ct.GetColorEntry(i) == colors[i]

    # --- RAT check ---
    if use_rat == "True":
        dst_rat = dst_band.GetDefaultRAT()
        assert dst_rat is not None
        assert dst_rat.GetRowCount() == rat.GetRowCount()
        assert dst_rat.GetNameOfCol(0) == "Value"
        assert dst_rat.GetNameOfCol(1) == "ClassName"
        assert dst_rat.GetNameOfCol(2) == "Real"

        for i in range(len(classname_list)):
            assert dst_rat.GetValueAsInt(i, 0) == i
            assert dst_rat.GetValueAsString(i, 1) == classname_list[i]
            assert dst_rat.GetValueAsDouble(i, 2) == classname_double[i]

    # --- Min / Max checks ---
    assert dst_band.ComputeRasterMinMax(False) == (1, 5)


@pytest.mark.parametrize(
    "data_type",
    init_type_list,
)
@pytest.mark.parametrize(
    "compress",
    ["YES", "NO"],
)
@pytest.mark.parametrize(
    "pattern",
    [None, "UserPattern"],
)
def test_miramonraster_multiband(tmp_path, data_type, compress, pattern):
    # --- Raster parameters ---
    xsize = 3
    ysize = 2
    nbands = 2
    geotransform = (100.0, 10.0, 0.0, 200.0, 0.0, -10.0)

    srs = osr.SpatialReference()
    epsg_code = 25831
    srs.ImportFromEPSG(epsg_code)
    wkt = srs.ExportToWkt()

    # --- Create in-memory dataset ---
    mem_driver = gdal.GetDriverByName("MEM")
    src_ds = mem_driver.Create("", xsize, ysize, nbands, data_type)

    src_ds.SetGeoTransform(geotransform)
    src_ds.SetProjection(wkt)

    # --- Create deterministic pixel values ---
    # band 1: 0..5
    # band 2: 100..105
    band1_values = list(range(xsize * ysize))
    band2_values = [v + 100 for v in band1_values]

    fmt1 = gdal_to_struct[data_type]
    fmt2 = gdal_to_struct[data_type]
    band1_bytes = struct.pack("<" + fmt1 * len(band1_values), *band1_values)
    band2_bytes = struct.pack("<" + fmt2 * len(band2_values), *band2_values)

    # --- Write raster data ---
    src_ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        xsize,
        ysize,
        band1_bytes,
        buf_xsize=xsize,
        buf_ysize=ysize,
        buf_type=data_type,
    )

    src_ds.GetRasterBand(2).WriteRaster(
        0,
        0,
        xsize,
        ysize,
        band2_bytes,
        buf_xsize=xsize,
        buf_ysize=ysize,
        buf_type=data_type,
    )

    for i in range(1, nbands + 1):
        band = src_ds.GetRasterBand(i)
        band.SetNoDataValue(0)
        band.FlushCache()

    band2 = src_ds.GetRasterBand(2)
    band2.SetUnitType("m")
    assert band2.GetUnitType() == "m"

    # --- Write to MiraMonRaster ---
    mm_path = tmp_path / "testI.rel"

    co = [f"COMPRESS={compress}"]
    if pattern is not None:
        co.append(f"PATTERN={pattern}")

    mm_driver = gdal.GetDriverByName("MiraMonRaster")
    assert mm_driver is not None
    mm_ds = mm_driver.CreateCopy(mm_path, src_ds, options=co)
    assert mm_ds is not None
    mm_ds = None

    # --- Reopen dataset ---
    dst_ds = gdal.Open(mm_path)
    assert dst_ds is not None
    assert dst_ds.GetDriver().ShortName == "MiraMonRaster"

    subdatasets = dst_ds.GetSubDatasets()
    assert subdatasets is None or len(subdatasets) == 0

    # --- Dataset checks ---
    assert dst_ds.RasterXSize == xsize
    assert dst_ds.RasterYSize == ysize
    assert dst_ds.RasterCount == 2
    assert dst_ds.GetGeoTransform() == geotransform

    # Comparing reference system
    if geotransform is not None:
        srs = dst_ds.GetSpatialRef()
        if (
            srs is not None
        ):  # in Fedora it returns None (but it's the only system it does)
            epsg_code = srs.GetAuthorityCode("PROJCS") or srs.GetAuthorityCode("GEOGCS")
            assert (
                epsg_code == epsg_code
            ), f"incorrect EPSG: {epsg_code}, waited {epsg_code}"

    # --- Pixel data checks ---
    dst_band1_bytes = dst_ds.GetRasterBand(1).ReadRaster(
        0, 0, xsize, ysize, buf_xsize=xsize, buf_ysize=ysize, buf_type=data_type
    )
    dst_band2_bytes = dst_ds.GetRasterBand(2).ReadRaster(
        0, 0, xsize, ysize, buf_xsize=xsize, buf_ysize=ysize, buf_type=data_type
    )

    assert dst_band1_bytes == band1_bytes
    assert dst_band2_bytes == band2_bytes

    dst_band1 = dst_ds.GetRasterBand(1)
    dst_band2 = dst_ds.GetRasterBand(2)

    # --- Min / Max checks ---
    assert dst_band1.ComputeRasterMinMax(False) == (1, 5)
    assert dst_band2.ComputeRasterMinMax(False) == (100, 105)

    assert dst_band2.GetUnitType() == "m"


def test_miramon_rgb_single_dataset(tmp_path):

    # ------------------------------------------------------------------
    # 1. Create MEM RGB dataset with primary colors
    # ------------------------------------------------------------------
    mem_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 3, gdal.GDT_Byte)

    mem_ds.GetRasterBand(1).SetColorInterpretation(gdal.GCI_RedBand)
    mem_ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_GreenBand)
    mem_ds.GetRasterBand(3).SetColorInterpretation(gdal.GCI_BlueBand)

    # Pixels:
    # X=0: Red   -> 255,0,0
    # X=1: Green -> 0,255,0
    # X=2: Blue  -> 0,0,255
    mem_ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, bytes([255, 0, 0]))
    mem_ds.GetRasterBand(2).WriteRaster(0, 0, 3, 1, bytes([0, 255, 0]))
    mem_ds.GetRasterBand(3).WriteRaster(0, 0, 3, 1, bytes([0, 0, 255]))

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    mem_ds.SetProjection(srs.ExportToWkt())
    mem_ds.SetGeoTransform((0, 1, 0, 0, 0, -1))

    # ------------------------------------------------------------------
    # 2. Create MiraMonRaster copy
    # ------------------------------------------------------------------
    out_base = tmp_path / "rgb_primary"

    drv = gdal.GetDriverByName("MiraMonRaster")
    assert drv is not None

    drv.CreateCopy(str(out_base), mem_ds)
    mem_ds = None

    # ------------------------------------------------------------------
    # 3. Check generated files (physical)
    # ------------------------------------------------------------------
    expected_files = {
        "rgb_primaryI.rel",
        "rgb_primary_R.img",
        "rgb_primary_G.img",
        "rgb_primary_B.img",
        "rgb_primary.mmm",
    }

    generated_files = {f.name for f in tmp_path.iterdir() if f.is_file()}

    assert expected_files == generated_files

    # ------------------------------------------------------------------
    # 4. Open logical dataset (.I.rel) and check bands
    # ------------------------------------------------------------------
    ds = gdal.Open(str(tmp_path / "rgb_primaryI.rel"))
    assert ds is not None
    assert ds.RasterCount == 3

    r = ds.GetRasterBand(1).ReadRaster(0, 0, 3, 1)
    g = ds.GetRasterBand(2).ReadRaster(0, 0, 3, 1)
    b = ds.GetRasterBand(3).ReadRaster(0, 0, 3, 1)

    assert r == bytes([255, 0, 0])
    assert g == bytes([0, 255, 0])
    assert b == bytes([0, 0, 255])
    ds = None


@pytest.mark.parametrize("separate_minmax", [True, False])
def test_miramon_raster_RAT_to_CT(tmp_path, separate_minmax):
    # --- Raster parameters ---
    xsize = 3
    ysize = 2

    # --- Create in-memory dataset ---
    mem_driver = gdal.GetDriverByName("MEM")
    src_ds = mem_driver.Create("", xsize, ysize, 1, gdal.GDT_Byte)

    # --- Create deterministic pixel values ---
    # band 1: 0..5
    band_values = list(range(xsize * ysize))
    fmt1 = gdal_to_struct[gdal.GDT_Byte]
    band_bytes = struct.pack("<" + fmt1 * len(band_values), *band_values)

    # --- Write raster data ---
    src_ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        xsize,
        ysize,
        band_bytes,
        buf_xsize=xsize,
        buf_ysize=ysize,
        buf_type=gdal.GDT_Byte,
    )

    band = src_ds.GetRasterBand(1)

    # ------------------------
    # 2. Create a RAT with RGB
    # columns to convert it into
    # a color table
    # ------------------------
    rat = gdal.RasterAttributeTable()

    # Create columns for RGB and min/max or VALUE
    if separate_minmax:
        rat.CreateColumn("MIN", gdal.GFT_Integer, gdal.GFU_Min)
        rat.CreateColumn("MAX", gdal.GFT_Integer, gdal.GFU_Max)
    else:
        rat.CreateColumn("VALUE", gdal.GFT_Integer, gdal.GFU_MinMax)
    rat.CreateColumn("R", gdal.GFT_Integer, gdal.GFU_Red)
    rat.CreateColumn("G", gdal.GFT_Integer, gdal.GFU_Green)
    rat.CreateColumn("B", gdal.GFT_Integer, gdal.GFU_Blue)
    rat.CreateColumn("Alpha", gdal.GFT_Integer, gdal.GFU_Alpha)

    # --- Color table ---

    for c in range(len(colors)):
        if separate_minmax:
            rat.SetValueAsInt(c, 0, int(c))
            rat.SetValueAsInt(c, 1, int(c + 1))
            for i in range(4):
                rat.SetValueAsInt(c, 2 + i, colors[c][i])
        else:
            rat.SetValueAsInt(c, 0, int(c))
            for i in range(4):
                rat.SetValueAsInt(c, 1 + i, colors[c][i])

    band.SetDefaultRAT(rat)

    # --- Write to MiraMonRaster ---
    mm_path = tmp_path / "RareTestI.rel"

    mm_driver = gdal.GetDriverByName("MiraMonRaster")
    assert mm_driver is not None
    mm_ds = mm_driver.CreateCopy(mm_path, src_ds)
    assert mm_ds is not None
    mm_ds = None

    # --- Reopen dataset ---
    dst_ds = gdal.Open(mm_path)
    assert dst_ds is not None
    assert dst_ds.GetDriver().ShortName == "MiraMonRaster"

    dst_band1 = dst_ds.GetRasterBand(1)
    assert dst_band1 is not None

    # --- Color table check ---
    dst_ct = dst_band1.GetRasterColorTable()
    assert dst_ct is not None
    for i in range(len(colors)):
        assert dst_ct.GetColorEntry(i) == colors[i]


def test_miramon_lineage_in_rel(tmp_path):
    # --- Raster parameters ---
    xsize = 3
    ysize = 2

    # --- Create in-memory dataset ---
    mem_driver = gdal.GetDriverByName("MEM")
    src_ds = mem_driver.Create("", xsize, ysize, 1, gdal.GDT_Byte)

    # --- Create deterministic pixel values ---
    # band 1: 0..5
    band_values = list(range(xsize * ysize))
    fmt1 = gdal_to_struct[gdal.GDT_Byte]
    band_bytes = struct.pack("<" + fmt1 * len(band_values), *band_values)

    # --- Write raster data ---
    src_ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        xsize,
        ysize,
        band_bytes,
        buf_xsize=xsize,
        buf_ysize=ysize,
        buf_type=gdal.GDT_Byte,
    )

    # --- Write to MiraMonRaster ---
    mm_path = tmp_path / "LineageTestI.rel"

    mm_driver = gdal.GetDriverByName("MiraMonRaster")
    assert mm_driver is not None
    mm_ds = mm_driver.CreateCopy(
        mm_path,
        src_ds,
        options=["COMPRESS=YES", "SRC_MDD=MIRAMON", "CATEGORICAL_BANDS=1"],
    )
    assert mm_ds is not None
    mm_ds = None

    # --- Lineage check ---
    # Just check that somelines appear in the .rel file, we don't need to check the exact content of them
    # just to check that the rel contains the lineage information in the expected format.
    with open(mm_path, "r") as f:
        content = f.read()
        assert "[QUALITY:LINEAGE]" in content
        assert "[QUALITY:LINEAGE:PROCESS1]" in content
        assert "purpose=GDAL process" in content
        assert "NomFitxer=" in content
        assert "[QUALITY:LINEAGE:PROCESS1:INOUT1]" in content
        assert "identifier=OutFile" in content
        assert "TypeValues=C" in content
        assert "identifier=-co COMPRESS" in content
        assert "ResultValue=YES" in content
        assert "identifier=-co SRC_MDD" in content
        assert "ResultValue=MIRAMON" in content
        assert "identifier=-co CATEGORICAL_BANDS" in content
        assert "ResultValue=1" in content


# Check that every line in expected_lineage_path
# is present in mm_path.
def check_lineage_content(expected_lineage_path, mm_path):

    # Read expected lineage file
    with io.open(expected_lineage_path, "r", encoding="cp1252") as f:
        expected_lines = [line.strip() for line in f]

    # Read MiraMon lineage file
    with io.open(mm_path, "r", encoding="cp1252") as f:
        mm_lines_set = set(line.strip() for line in f)

    # Check that every expected line exists in mm file
    for line in expected_lines:
        if line not in mm_lines_set:
            assert False, f"Missing expected lineage line: {line!r}"


def test_miramon_lineage_preservation(tmp_path):

    # --- Open existing MiraMonRaster dataset with lineage information ---
    mm_ori_path = "data/miramon/lineage/int_2x3_6_RLEI.rel"
    expected_lineage_path = "data/miramon/lineage/int_2x3_6_RLE_expected_lineage.txt"
    src_ds = gdal.OpenEx(mm_ori_path, allowed_drivers=["MiraMonRaster"])
    assert src_ds is not None, "Could not open the file"

    # --- Write to VRT preserving the MIRAMON metadata domain ---
    vrt_path = tmp_path / "LineagePreservation.vrt"

    vrt_driver = gdal.GetDriverByName("VRT")
    assert vrt_driver is not None
    mm_ds = vrt_driver.CreateCopy(
        vrt_path,
        src_ds,
        options=["SRC_MDD=MIRAMON"],
    )
    assert mm_ds is not None
    mm_ds = None

    # --- Reopen the VRT and write it back to a new MiraMonRaster ---
    mm_ds = gdal.Open(vrt_path)
    assert mm_ds is not None, "Could not open the VRT file"

    # --- Write back to MiraMonRaster ---
    mm_path = tmp_path / "LineagePreservationI.rel"

    mm_driver = gdal.GetDriverByName("MiraMonRaster")
    assert mm_driver is not None
    mm_lineage = mm_driver.CreateCopy(mm_path, mm_ds, options=["SRC_MDD=MIRAMON"])
    assert mm_lineage is not None
    mm_lineage = None

    # --- Lineage check ---
    # Just check that somelines appear in the .rel file, we don't need to check the exact content of them
    # just to check that the rel contains the lineage information in the expected format.
    check_lineage_content(expected_lineage_path, mm_path)


gdal_to_struct = {
    gdal.GDT_UInt8: "B",
    gdal.GDT_Int16: "h",
    gdal.GDT_UInt16: "H",
    gdal.GDT_Int32: "i",
    gdal.GDT_Float32: "f",
    gdal.GDT_Float64: "d",
}

init_type_list = [
    gdal.GDT_UInt8,
    gdal.GDT_Int16,
    gdal.GDT_UInt16,
    gdal.GDT_Int32,
    gdal.GDT_Float32,
    gdal.GDT_Float64,
]


@pytest.mark.parametrize(
    "data_type",
    init_type_list,
)
@pytest.mark.parametrize(
    "compress",
    ["YES", "NO"],
)
def test_miramonraster_compress(tmp_path, data_type, compress):

    # --- Raster parameters ---
    xsize = 5
    ysize = 2

    # --- Create in-memory dataset ---
    mem_driver = gdal.GetDriverByName("MEM")
    src_ds = mem_driver.Create("", xsize, ysize, 1, data_type)

    band_values = [0, 1, 2, 3, 4, 5, 5, 5, 5, 5]

    fmt = gdal_to_struct[data_type]
    band_bytes = struct.pack("<" + fmt * len(band_values), *band_values)

    # --- Write raster data ---
    src_ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        xsize,
        ysize,
        band_bytes,
        buf_xsize=xsize,
        buf_ysize=ysize,
        buf_type=data_type,
    )

    band = src_ds.GetRasterBand(1)
    band.FlushCache()

    # --- Write to MiraMonRaster ---
    mm_path = tmp_path / "testI.rel"

    co = [f"COMPRESS={compress}"]

    mm_driver = gdal.GetDriverByName("MiraMonRaster")
    assert mm_driver is not None
    mm_ds = mm_driver.CreateCopy(mm_path, src_ds, options=co)
    assert mm_ds is not None
    mm_ds = None

    # --- Reopen dataset ---
    dst_ds = gdal.Open(mm_path)
    assert dst_ds is not None
    assert dst_ds.GetDriver().ShortName == "MiraMonRaster"

    subdatasets = dst_ds.GetSubDatasets()
    assert subdatasets is None or len(subdatasets) == 0

    # --- Dataset checks ---
    assert dst_ds.RasterXSize == xsize
    assert dst_ds.RasterYSize == ysize
    assert dst_ds.RasterCount == 1

    # --- Pixel data checks ---
    dst_band_bytes = dst_ds.GetRasterBand(1).ReadRaster(
        0, 0, xsize, ysize, buf_xsize=xsize, buf_ysize=ysize, buf_type=data_type
    )

    assert dst_band_bytes == band_bytes

    dst_band = dst_ds.GetRasterBand(1)

    # --- Min / Max checks ---
    assert dst_band.ComputeRasterMinMax(False) == (0, 5)
