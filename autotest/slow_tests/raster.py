#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Raster testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
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

import os
import sys

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.slow


def test_translate_vrt_with_complex_source(tmp_path):

    metatile_filename = str(tmp_path / "metatile.tif")
    metatile_width = 4096
    metatile_height = 4096
    blockxsize = 256
    blockysize = 256
    tile_ds = gdal.GetDriverByName("GTiff").Create(
        metatile_filename,
        metatile_width,
        metatile_height,
        3,
        options=[
            "COMPRESS=LZW",
            "TILED=YES",
            f"BLOCKXSIZE={blockxsize}",
            f"BLOCKYSIZE={blockysize}",
        ],
    )
    with gdaltest.config_option("GDAL_TIFF_INTERNAL_MASK", "YES"):
        tile_ds.GetRasterBand(1).CreateMaskBand(gdal.GMF_PER_DATASET)
    for j in range(metatile_height // blockysize):
        for i in range(metatile_width // blockxsize):
            block_pixel_count = blockxsize * blockysize
            data = ((3 * (i + j)) % 255).to_bytes(
                length=1, byteorder="little"
            ) * block_pixel_count
            data += ((3 * (i + j) + 1) % 255).to_bytes(
                length=1, byteorder="little"
            ) * block_pixel_count
            data += ((3 * (i + j) + 2) % 255).to_bytes(
                length=1, byteorder="little"
            ) * block_pixel_count
            tile_ds.WriteRaster(
                i * blockxsize, j * blockysize, blockxsize, blockysize, data
            )
            data = (255).to_bytes(length=1, byteorder="little") * block_pixel_count
            tile_ds.GetRasterBand(1).GetMaskBand().WriteRaster(
                i * blockxsize, j * blockysize, blockxsize, blockysize, data
            )
    del tile_ds

    metatile_x_count = 30
    metatile_y_count = 30
    vrtxsize = metatile_width * metatile_x_count
    vrtysize = metatile_height * metatile_y_count
    vrt = f"""<VRTDataset rasterXSize="{vrtxsize}" rasterYSize="{vrtysize}">"""
    vrt_band_1 = """<VRTRasterBand dataType="Byte" band="1">"""
    vrt_band_2 = """<VRTRasterBand dataType="Byte" band="2">"""
    vrt_band_3 = """<VRTRasterBand dataType="Byte" band="3">"""
    vrt_band_mask = """<VRTRasterBand dataType="Byte" band="mask">"""
    for j in range(metatile_y_count):
        for i in range(metatile_x_count):
            yoff = j * metatile_height
            xoff = i * metatile_width
            filename = "metatile.tif"
            if sys.platform != "win32":
                new_filename = str(tmp_path / f"metatile_{i}_{j}.tif")
                os.symlink(filename, new_filename)
                filename = new_filename

            source = f"""<ComplexSource resampling="bilinear">
              <SourceFilename relativeToVRT="1">{filename}</SourceFilename>
              <SourceBand>1</SourceBand>
              <SrcRect xOff="0" yOff="0" xSize="{metatile_width}" ySize="{metatile_height}" />
              <DstRect xOff="{xoff}" yOff="{yoff}" xSize="{metatile_width}" ySize="{metatile_height}" />
              <UseMaskBand>true</UseMaskBand>
            </ComplexSource>"""
            vrt_band_1 += source
            vrt_band_2 += source.replace(
                "<SourceBand>1</SourceBand>", "<SourceBand>2</SourceBand>"
            )
            vrt_band_3 += source.replace(
                "<SourceBand>1</SourceBand>", "<SourceBand>3</SourceBand>"
            )
            vrt_band_mask += source.replace(
                "<SourceBand>1</SourceBand>", "<SourceBand>mask,1</SourceBand>"
            )
    vrt += vrt_band_1
    vrt += "</VRTRasterBand>"
    vrt += vrt_band_2
    vrt += "</VRTRasterBand>"
    vrt += vrt_band_3
    vrt += "</VRTRasterBand>"
    vrt += "<MaskBand>"
    vrt += vrt_band_mask
    vrt += "</VRTRasterBand>"
    vrt += "</MaskBand>"
    vrt += "</VRTDataset>"

    vrt_filename = str(tmp_path / "test.vrt")
    open(vrt_filename, "wt").write(vrt)

    out_filename = str(tmp_path / "out.tif")

    with gdaltest.config_option("GDAL_TIFF_INTERNAL_MASK", "YES"):
        gdal.Translate(
            out_filename,
            vrt_filename,
            creationOptions=[
                "BIGTIFF=YES",
                "TILED=YES",
                "COMPRESS=DEFLATE",
                "NUM_THREADS=ALL_CPUS",
            ],
            callback=gdal.TermProgress,
        )

    tile_ds = gdal.Open(metatile_filename)
    tile_data = tile_ds.ReadRaster()
    del tile_ds
    out_ds = gdal.Open(out_filename)
    for i in range(metatile_x_count):
        yoff = i * metatile_height
        xoff = i * metatile_width
        assert (
            out_ds.ReadRaster(xoff, yoff, metatile_width, metatile_height) == tile_data
        )


@pytest.mark.require_driver("GPKG")
def test_translate_vrtti(tmp_path):

    tile_filename = str(tmp_path / "tile.tif")
    tile_width = 1
    tile_height = 1
    tile_ds = gdal.GetDriverByName("GTiff").Create(
        tile_filename, tile_width, tile_height, 3
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    tile_ds.SetSpatialRef(srs)
    tile_ds.GetRasterBand(1).Fill(1)
    tile_ds.GetRasterBand(2).Fill(2)
    tile_ds.GetRasterBand(3).Fill(3)
    del tile_ds

    print("Creating tile index...")
    tileindex = str(tmp_path / "tileindex.gti.gpkg")
    ds = ogr.GetDriverByName("GPKG").CreateDataSource(tileindex)
    lyr = ds.CreateLayer("index", geom_type=ogr.wkbPolygon, srs=srs)
    lyr.CreateField(ogr.FieldDefn("location", ogr.OFTString))
    lyr.SetMetadataItem("RESX", "0.001")
    lyr.SetMetadataItem("RESY", "0.001")
    lyr.SetMetadataItem("BAND_COUNT", "3")
    lyr.SetMetadataItem("DATA_TYPE", "Byte")
    lyr.StartTransaction()
    tile_y_count = 200
    tile_x_count = 200
    resx = 0.001
    resy = 0.001
    for j in range(tile_y_count):
        for i in range(tile_x_count):
            minx = i * resx
            miny = j * resy
            maxx = minx + resx
            maxy = miny + resy
            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetField(
                "location", f"vrt://{tile_filename}?a_ullr={minx},{maxy},{maxx},{miny}"
            )
            f.SetGeometry(
                ogr.CreateGeometryFromWkt(
                    f"POLYGON(({minx} {miny},{minx} {maxy},{maxx} {maxy},{maxx} {miny},{minx} {miny}))"
                )
            )
            lyr.CreateFeature(f)
    lyr.CommitTransaction()
    del ds
    print("... done")

    print("Reading tile index...")
    ds = gdal.Open(tileindex)
    assert ds.RasterXSize == tile_x_count
    assert ds.RasterYSize == tile_y_count
    assert ds.GetGeoTransform() == pytest.approx(
        (0.0, resx, 0.0, resy * tile_y_count, 0.0, -resy)
    )
    assert ds.ReadRaster() == b"\x01" * (tile_y_count * tile_x_count) + b"\x02" * (
        tile_y_count * tile_x_count
    ) + b"\x03" * (tile_y_count * tile_x_count)
