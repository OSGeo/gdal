#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VRTPansharpenedDataset support.
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import shutil
import struct

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)


@pytest.fixture(autouse=True, scope="module")
def startup_and_cleanup():

    src_ds = gdal.Open("data/small_world.tif")
    src_data = src_ds.GetRasterBand(1).ReadRaster()
    gt = src_ds.GetGeoTransform()
    wkt = src_ds.GetProjectionRef()
    src_ds = None
    pan_ds = gdal.GetDriverByName("GTiff").Create("tmp/small_world_pan.tif", 800, 400)
    gt = [gt[i] for i in range(len(gt))]
    gt[1] *= 0.5
    gt[5] *= 0.5
    pan_ds.SetGeoTransform(gt)
    pan_ds.SetProjection(wkt)
    pan_ds.GetRasterBand(1).WriteRaster(0, 0, 800, 400, src_data, 400, 200)
    pan_ds = None

    yield

    gdal.GetDriverByName("GTiff").Delete("tmp/small_world_pan.tif")
    if gdal.VSIStatL("tmp/small_world.tif"):
        gdal.GetDriverByName("GTiff").Delete("tmp/small_world.tif")
    if gdal.VSIStatL("/vsimem/pan.tif"):
        gdal.GetDriverByName("GTiff").Delete("/vsimem/pan.tif")
    if gdal.VSIStatL("/vsimem/ms.tif"):
        gdal.GetDriverByName("GTiff").Delete("/vsimem/ms.tif")


###############################################################################
# Error cases


@gdaltest.disable_exceptions()
def test_vrtpansharpen_1():

    # Missing PansharpeningOptions
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # PanchroBand missing
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # PanchroBand.SourceFilename missing
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # Invalid dataset name
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="0">/does/not/exist</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # Inconsistent declared VRT dimensions with panchro dataset.
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="1800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # VRTRasterBand of unrecognized subclass 'blabla'
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="blabla">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # Algorithm unsupported_alg unsupported
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>unsupported_alg</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # 10 invalid band of tmp/small_world_pan.tif
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>10</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # SpectralBand.dstBand = '-1' invalid
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="-1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # SpectralBand.SourceFilename missing
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # Invalid dataset name
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">/does/not/exist</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # 10 invalid band of data/small_world.tif
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>10</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # Another spectral band is already mapped to output band 1
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # No spectral band defined
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # Hole in SpectralBand.dstBand numbering
    with gdal.quiet_errors():
        vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="4">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""")
    assert vrt_ds is None

    # Band 4 of type VRTPansharpenedRasterBand, but no corresponding SpectralBand
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="4" subClass="VRTPansharpenedRasterBand">
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # SpectralBand.dstBand = '3' invalid
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # 2 weights defined, but 3 input spectral bands
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is None

    # Dimensions of input spectral band 1 different from first spectral band
    with gdal.quiet_errors():
        vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""")
    assert vrt_ds is None

    # Just warnings
    # Warning 1: Pan dataset and data/byte.tif do not seem to have same projection. Results might be incorrect
    # Georeferencing of top-left corner of pan dataset and data/byte.tif do not match
    # Georeferencing of bottom-right corner of pan dataset and data/byte.tif do not match
    gdal.ErrorReset()
    with gdal.quiet_errors():
        vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <SpatialExtentAdjustment>None</SpatialExtentAdjustment>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/byte.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""")
    assert vrt_ds is not None
    assert gdal.GetLastErrorMsg() != ""

    # Just warnings
    # No spectral band is mapped to an output band
    # No output pansharpened band defined
    gdal.ErrorReset()
    with gdal.quiet_errors():
        vrt_ds = gdal.Open(
            """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.33333,0.333333,0.333333</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand>
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand>
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand>
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>"""
        )
    assert vrt_ds is not None
    assert gdal.GetLastErrorMsg() != ""

    # Unsupported
    with gdal.quiet_errors():
        ret = vrt_ds.AddBand(gdal.GDT_UInt8)
    assert ret != 0


###############################################################################
# Nominal cases


def test_vrtpansharpen_2():

    shutil.copy("data/small_world.tif", "tmp/small_world.tif")

    # Super verbose case
    vrt_ds = gdal.Open(
        """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
    <SRS>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]</SRS>
    <GeoTransform> -1.8000000000000000e+02,  4.5000000000000001e-01,  0.0000000000000000e+00,  9.0000000000000000e+01,  0.0000000000000000e+00, -4.5000000000000001e-01</GeoTransform>
    <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Red</ColorInterp>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Green</ColorInterp>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Blue</ColorInterp>
    </VRTRasterBand>
    <PansharpeningOptions>
        <Algorithm>WeightedBrovey</Algorithm>
        <AlgorithmOptions>
            <Weights>0.33333333333333333,0.33333333333333333,0.33333333333333333</Weights>
        </AlgorithmOptions>
        <Resampling>Cubic</Resampling>
        <NumThreads>ALL_CPUS</NumThreads>
        <BitDepth>8</BitDepth>
        <PanchroBand>
                <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    )
    assert vrt_ds is not None
    assert vrt_ds.GetFileList() == ["tmp/small_world_pan.tif", "data/small_world.tif"]
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("NBITS", "IMAGE_STRUCTURE") is None
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    expected_cs = (
        [4735, 10000, 9742],
        [4731, 9991, 9734],
        [4726, 10004, 9727],  # ICC 2004.0.2 in -O3
    )
    assert cs in expected_cs
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 0
    assert vrt_ds.GetRasterBand(1).GetOverview(-1) is None
    assert vrt_ds.GetRasterBand(1).GetOverview(0) is None

    # Check VRTPansharpenedDataset::IRasterIO() in non-resampling case
    data = vrt_ds.ReadRaster()
    tmp_ds = gdal.GetDriverByName("MEM").Create("", 800, 400, 3)
    tmp_ds.WriteRaster(0, 0, 800, 400, data)
    cs = [tmp_ds.GetRasterBand(i + 1).Checksum() for i in range(tmp_ds.RasterCount)]
    assert cs in expected_cs

    # Check VRTPansharpenedDataset::IRasterIO() in resampling case
    data = vrt_ds.ReadRaster(0, 0, 800, 400, 400, 200)
    ref_data = tmp_ds.ReadRaster(0, 0, 800, 400, 400, 200)
    assert data == ref_data

    # Compact case
    vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>""")
    assert vrt_ds is not None
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    assert cs in expected_cs

    # Expose pan band too
    vrt_ds = gdal.Open(
        """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
    <SRS>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]</SRS>
    <GeoTransform> -1.8000000000000000e+02,  4.5000000000000001e-01,  0.0000000000000000e+00,  9.0000000000000000e+01,  0.0000000000000000e+00, -4.5000000000000001e-01</GeoTransform>
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
            <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
            <SourceBand>1</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Red</ColorInterp>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Green</ColorInterp>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="4" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Blue</ColorInterp>
    </VRTRasterBand>
    <PansharpeningOptions>
        <Algorithm>WeightedBrovey</Algorithm>
        <AlgorithmOptions>
            <Weights>0.33333333333333333,0.33333333333333333,0.33333333333333333</Weights>
        </AlgorithmOptions>
        <Resampling>Cubic</Resampling>
        <NumThreads>ALL_CPUS</NumThreads>
        <BitDepth>8</BitDepth>
        <PanchroBand>
                <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="4">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    )
    assert vrt_ds is not None
    # gdal.GetDriverByName('GTiff').CreateCopy('out1.tif', vrt_ds)
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    assert cs in (
        [50261, 4735, 10000, 9742],
        [50261, 4731, 9991, 9734],
        [50261, 4726, 10004, 9727],  # ICC 2004.0.2 in -O3
    )

    # Same, but everything scrambled, and with spectral bands not in
    # the same dataset
    vrt_ds = gdal.Open(
        """<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
    <SRS>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]</SRS>
    <GeoTransform> -1.8000000000000000e+02,  4.5000000000000001e-01,  0.0000000000000000e+00,  9.0000000000000000e+01,  0.0000000000000000e+00, -4.5000000000000001e-01</GeoTransform>
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
            <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
            <SourceBand>1</SourceBand>
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Red</ColorInterp>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Green</ColorInterp>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="4" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Blue</ColorInterp>
    </VRTRasterBand>
    <PansharpeningOptions>
        <Algorithm>WeightedBrovey</Algorithm>
        <AlgorithmOptions>
            <Weights>0.33333333333333333,0.33333333333333333,0.33333333333333333</Weights>
        </AlgorithmOptions>
        <Resampling>Cubic</Resampling>
        <NumThreads>ALL_CPUS</NumThreads>
        <BitDepth>8</BitDepth>
        <PanchroBand>
                <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="4">
                <SourceFilename relativeToVRT="1">tmp/small_world.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    )
    assert vrt_ds is not None
    # gdal.GetDriverByName('GTiff').CreateCopy('out2.tif', vrt_ds)
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    assert cs in (
        [50261, 4735, 10000, 9742],
        [50261, 4727, 9998, 9732],
        [50261, 4729, 10004, 9727],  # ICC 2004.0.2 in -O3
    )


###############################################################################
# Test with overviews


def test_vrtpansharpen_3(tmp_vsimem):

    shutil.copy("data/small_world.tif", "tmp/small_world.tif")

    ds = gdal.Open("tmp/small_world_pan.tif")
    ds.BuildOverviews("CUBIC", [2])
    ds = None

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename relativeToVRT="1">tmp/small_world.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1">tmp/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1">tmp/small_world.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""

    # Test when only Pan band has overviews
    vrt_ds = gdal.Open(xml)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 0

    vrt_ds = None

    ds = gdal.Open("tmp/small_world.tif")
    ds.BuildOverviews("CUBIC", [2])
    ds = None

    # Test when both Pan and spectral bands have overviews
    vrt_ds = gdal.Open(xml)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert vrt_ds.GetRasterBand(1).GetOverview(0) is not None
    cs = [
        vrt_ds.GetRasterBand(i + 1).GetOverview(0).Checksum()
        for i in range(vrt_ds.RasterCount)
    ]
    assert cs in (
        [18033, 18395, 16824],
        [18033, 18395, 16822],
        [18032, 18399, 16825],  # ICC 2004.0.2 in -O3
    )

    vrt_ds = None

    # Now test when the spatial extent of the PAN and MS datasets is different
    # and we create a in-memory VRT to make them consistent.
    gdal.Translate(
        "tmp/small_world_pan_cropped.vrt",
        "tmp/small_world_pan.tif",
        options="-srcwin 10 10 780 380",
    )

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename relativeToVRT="1">tmp/small_world_pan_cropped.vrt</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename relativeToVRT="1">tmp/small_world.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1">tmp/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1">tmp/small_world.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""

    vrt_ds = gdal.Open(xml)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1
    vrt_ds = None

    gdal.Unlink("tmp/small_world_pan_cropped.vrt")
    gdal.Unlink("tmp/small_world_pan.tif.ovr")
    gdal.Unlink("tmp/small_world.tif.ovr")


###############################################################################
# Test RasterIO() with various buffer datatypes


def test_vrtpansharpen_4():

    shutil.copy("data/small_world.tif", "tmp/small_world.tif")

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename relativeToVRT="1">tmp/small_world.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1">tmp/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1">tmp/small_world.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""

    vrt_ds = gdal.Open(xml)
    for dt in [
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        gdal.GDT_UInt32,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
        gdal.GDT_CFloat64,
    ]:
        data = vrt_ds.GetRasterBand(1).ReadRaster(buf_type=dt)
        tmp_ds = gdal.GetDriverByName("MEM").Create("", 800, 400, 1, dt)
        tmp_ds.WriteRaster(0, 0, 800, 400, data)
        cs = tmp_ds.GetRasterBand(1).Checksum()
        if dt == gdal.GDT_CFloat64:
            expected_cs = [4724, 4720, 4756]  # ICC 2004.0.2 in -O3
        else:
            expected_cs = [4735, 4731, 4726]  # ICC 2004.0.2 in -O3
        assert cs in expected_cs, gdal.GetDataTypeName(dt)


###############################################################################
# Test RasterIO() with various band datatypes


def test_vrtpansharpen_5():

    for dt in [
        gdal.GDT_Int16,
        gdal.GDT_UInt16,
        gdal.GDT_Int32,
        gdal.GDT_UInt32,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
        gdal.GDT_CFloat64,
    ]:

        spectral_xml = """<VRTDataset rasterXSize="400" rasterYSize="200">
  <SRS>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]</SRS>
  <GeoTransform> -1.8000000000000000e+02,  9.0000000000000002e-01,  0.0000000000000000e+00,  9.0000000000000000e+01,  0.0000000000000000e+00, -9.0000000000000002e-01</GeoTransform>
  <VRTRasterBand dataType="%s" band="1">
    <ColorInterp>Red</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/small_world.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="%s" band="2">
    <ColorInterp>Green</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/small_world.tif</SourceFilename>
      <SourceBand>2</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="%s" band="3">
    <ColorInterp>Blue</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/small_world.tif</SourceFilename>
      <SourceBand>3</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""" % (
            gdal.GetDataTypeName(dt),
            gdal.GetDataTypeName(dt),
            gdal.GetDataTypeName(dt),
        )

        xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename relativeToVRT="1"><![CDATA[<VRTDataset rasterXSize="800" rasterYSize="400">
<SRS>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]</SRS>
<GeoTransform> -1.8000000000000000e+02,  4.5000000000000001e-01,  0.0000000000000000e+00,  9.0000000000000000e+01,  0.0000000000000000e+00, -4.5000000000000001e-01</GeoTransform>
<VRTRasterBand dataType="%s" band="1">
    <SimpleSource>
    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
    <SourceBand>1</SourceBand>
    </SimpleSource>
</VRTRasterBand>
</VRTDataset>]]></SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename relativeToVRT="1"><![CDATA[%s]]></SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1"><![CDATA[%s]]></SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1"><![CDATA[%s]]></SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>""" % (
            gdal.GetDataTypeName(dt),
            spectral_xml,
            spectral_xml,
            spectral_xml,
        )

        vrt_ds = gdal.Open(xml)
        data = vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt8)
        tmp_ds = gdal.GetDriverByName("MEM").Create("", 800, 400, 1)
        tmp_ds.WriteRaster(0, 0, 800, 400, data)
        cs = tmp_ds.GetRasterBand(1).Checksum()
        if dt == gdal.GDT_UInt16:
            assert cs in (
                4553,
                4549,
                4544,  # ICC 2004.0.2 in -O3
            ), gdal.GetDataTypeName(dt)
        else:
            assert cs == 4450, gdal.GetDataTypeName(dt)


###############################################################################
# Test BitDepth limitations


def test_vrtpansharpen_6():

    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")

    # i = 0: VRT has <BitDepth>7</BitDepth>
    # i = 1: bands have NBITS=7 and VRT <BitDepth>7</BitDepth>
    # i = 2: bands have NBITS=7
    for dt in [gdal.GDT_UInt8, gdal.GDT_UInt16]:
        if dt == gdal.GDT_UInt8:
            nbits = 7
        elif dt == gdal.GDT_UInt16:
            nbits = 12
        else:
            nbits = 17
        for i in range(3):
            if i > 0:
                options = ["NBITS=%d" % nbits]
            else:
                options = []
            mem_ds = gdal.GetDriverByName("GTiff").Create(
                "/vsimem/ms.tif", 4, 1, 1, dt, options=options
            )
            mem_ds.SetGeoTransform([0, 1, 0, 0, 0, 1])
            ar = numpy.array([[80, 125, 125, 80]])
            if dt == gdal.GDT_UInt16:
                ar = ar << (12 - 7)
            elif dt == gdal.GDT_UInt32:
                ar = ar << (17 - 7)
            mem_ds.GetRasterBand(1).WriteArray(ar)
            mem_ds = None

            mem_ds = gdal.GetDriverByName("GTiff").Create(
                "/vsimem/pan.tif", 8, 2, 1, dt, options=options
            )
            mem_ds.SetGeoTransform([0, 0.5, 0, 0, 0, 0.5])
            ar = numpy.array(
                [
                    [76, 89, 115, 127, 127, 115, 89, 76],
                    [76, 89, 115, 127, 127, 115, 89, 76],
                ]
            )
            if dt == gdal.GDT_UInt16:
                ar = ar << (12 - 7)
            elif dt == gdal.GDT_UInt32:
                ar = ar << (17 - 7)
            mem_ds.GetRasterBand(1).WriteArray(ar)
            mem_ds = None

            xml = """<VRTDataset subClass="VRTPansharpenedDataset">
            <PansharpeningOptions>"""
            if i < 2:
                xml += """            <BitDepth>%d</BitDepth>""" % nbits
            xml += """            <AlgorithmOptions><Weights>0.8</Weights></AlgorithmOptions>
                <PanchroBand>
                        <SourceFilename>/vsimem/pan.tif</SourceFilename>
                        <SourceBand>1</SourceBand>
                </PanchroBand>
                <SpectralBand dstBand="1">
                        <SourceFilename>/vsimem/ms.tif</SourceFilename>
                        <SourceBand>1</SourceBand>
                </SpectralBand>
            </PansharpeningOptions>
        </VRTDataset>"""

            vrt_ds = gdal.Open(xml)
            assert vrt_ds.GetRasterBand(1).GetMetadataItem(
                "NBITS", "IMAGE_STRUCTURE"
            ) == str(nbits)

            ar = vrt_ds.GetRasterBand(1).ReadAsArray()
            if dt == gdal.GDT_UInt8:
                expected_ar = [95, 111, 127, 127, 127, 127, 111, 95]
            elif dt == gdal.GDT_UInt16:
                expected_ar = [3040, 3560, 4095, 4095, 4095, 4095, 3560, 3040]
            else:
                expected_ar = [
                    97280,
                    113920,
                    131071,
                    131071,
                    131071,
                    131071,
                    113920,
                    97280,
                ]

            if list(ar[0]) != expected_ar:
                print(gdal.GetDataTypeName(dt))
                pytest.fail(i)
            vrt_ds = None

            gdal.Unlink("/vsimem/ms.tif")
            gdal.Unlink("/vsimem/pan.tif")


###############################################################################
# Test bands with different extents


@gdaltest.disable_exceptions()
def test_vrtpansharpen_7():

    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/vrtpansharpen_7_pan.tif", 20, 40)
    ds.SetGeoTransform([120, 1, 0, 80, 0, -1])
    ds = None

    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/vrtpansharpen_7_ms.tif", 15, 30)
    ds.SetGeoTransform([100, 2, 0, 100, 0, -2])
    ds = None

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename>/vsimem/vrtpansharpen_7_pan.tif</SourceFilename>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>/vsimem/vrtpansharpen_7_ms.tif</SourceFilename>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    ds = gdal.Open(xml)
    assert (
        ds.GetGeoTransform() == (100.0, 1.0, 0.0, 100.0, 0.0, -1.0)
        and ds.RasterXSize == 40
        and ds.RasterYSize == 60
    )
    ds = None

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <SpatialExtentAdjustment>Union</SpatialExtentAdjustment>
        <PanchroBand>
                <SourceFilename>/vsimem/vrtpansharpen_7_pan.tif</SourceFilename>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>/vsimem/vrtpansharpen_7_ms.tif</SourceFilename>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    ds = gdal.Open(xml)
    assert (
        ds.GetGeoTransform() == (100.0, 1.0, 0.0, 100.0, 0.0, -1.0)
        and ds.RasterXSize == 40
        and ds.RasterYSize == 60
    )
    ds = None

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <SpatialExtentAdjustment>BlaBla</SpatialExtentAdjustment>
        <PanchroBand>
                <SourceFilename>/vsimem/vrtpansharpen_7_pan.tif</SourceFilename>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>/vsimem/vrtpansharpen_7_ms.tif</SourceFilename>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.Open(xml)
    assert (
        ds.GetGeoTransform() == (100.0, 1.0, 0.0, 100.0, 0.0, -1.0)
        and ds.RasterXSize == 40
        and ds.RasterYSize == 60
    )
    assert gdal.GetLastErrorMsg() == ""
    ds = None

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <SpatialExtentAdjustment>Intersection</SpatialExtentAdjustment>
        <PanchroBand>
                <SourceFilename>/vsimem/vrtpansharpen_7_pan.tif</SourceFilename>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>/vsimem/vrtpansharpen_7_ms.tif</SourceFilename>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    ds = gdal.Open(xml)
    assert (
        ds.GetGeoTransform() == (120.0, 1.0, 0.0, 80.0, 0.0, -1.0)
        and ds.RasterXSize == 10
        and ds.RasterYSize == 40
    )
    ds = None

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <SpatialExtentAdjustment>None</SpatialExtentAdjustment>
        <PanchroBand>
                <SourceFilename>/vsimem/vrtpansharpen_7_pan.tif</SourceFilename>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>/vsimem/vrtpansharpen_7_ms.tif</SourceFilename>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    gdal.ErrorReset()
    with gdal.quiet_errors():
        ds = gdal.Open(xml)
    assert (
        ds.GetGeoTransform() == (120.0, 1.0, 0.0, 80.0, 0.0, -1.0)
        and ds.RasterXSize == 20
        and ds.RasterYSize == 40
    )
    assert gdal.GetLastErrorMsg() != ""
    ds = None

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <SpatialExtentAdjustment>NoneWithoutWarning</SpatialExtentAdjustment>
        <PanchroBand>
                <SourceFilename>/vsimem/vrtpansharpen_7_pan.tif</SourceFilename>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>/vsimem/vrtpansharpen_7_ms.tif</SourceFilename>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    gdal.ErrorReset()
    ds = gdal.Open(xml)
    assert (
        ds.GetGeoTransform() == (120.0, 1.0, 0.0, 80.0, 0.0, -1.0)
        and ds.RasterXSize == 20
        and ds.RasterYSize == 40
    )
    assert gdal.GetLastErrorMsg() == ""
    ds = None

    # Empty intersection
    ds = gdal.GetDriverByName("GTiff").Create("/vsimem/vrtpansharpen_7_ms.tif", 15, 30)
    ds.SetGeoTransform([-100, 2, 0, -100, 0, -2])
    ds = None

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <SpatialExtentAdjustment>Intersection</SpatialExtentAdjustment>
        <PanchroBand>
                <SourceFilename>/vsimem/vrtpansharpen_7_pan.tif</SourceFilename>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>/vsimem/vrtpansharpen_7_ms.tif</SourceFilename>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    with gdal.quiet_errors():
        ds = gdal.Open(xml)
    assert ds is None
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/vrtpansharpen_7_pan.tif")
    gdal.GetDriverByName("GTiff").Delete("/vsimem/vrtpansharpen_7_ms.tif")


###############################################################################
# Test bands with different extents


def test_vrtpansharpen_band_with_different_extents():

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename>tmp/small_world_pan.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>data/small_world.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename>data/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename>data/small_world.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""

    vrt_ds = gdal.Open(xml)

    gdal.Translate(
        "/vsimem/small_world_pan_extended.vrt",
        "tmp/small_world_pan.tif",
        options="-srcwin -100 -200 950 700",
    )

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename>/vsimem/small_world_pan_extended.vrt</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>data/small_world.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename>data/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename>data/small_world.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""

    vrt_extended_ds = gdal.Open(xml)
    assert struct.unpack("B" * 3, vrt_extended_ds.ReadRaster(0, 0, 1, 1)) == (0, 0, 0)
    assert struct.unpack(
        "B" * 3, vrt_extended_ds.ReadRaster(vrt_extended_ds.RasterXSize - 1, 0, 1, 1)
    ) == (0, 0, 0)
    assert struct.unpack(
        "B" * 3, vrt_extended_ds.ReadRaster(0, vrt_extended_ds.RasterYSize - 1, 1, 1)
    ) == (0, 0, 0)
    assert struct.unpack(
        "B" * 3,
        vrt_extended_ds.ReadRaster(
            vrt_extended_ds.RasterXSize - 1, vrt_extended_ds.RasterYSize - 1, 1, 1
        ),
    ) == (0, 0, 0)
    assert struct.unpack(
        "B" * 3,
        vrt_extended_ds.ReadRaster(
            vrt_extended_ds.RasterXSize // 2, vrt_extended_ds.RasterYSize // 2, 1, 1
        ),
    ) != (0, 0, 0)

    # Check that the intersecting parts of the nominal and the extended
    # pansharpened datasets have very similar content (will be slightly
    # due to interpolation differences near the edges)
    tmp_ds = gdal.GetDriverByName("MEM").Create("", 800, 400, 3)
    tmp_ds.WriteRaster(0, 0, 800, 400, vrt_extended_ds.ReadRaster(100, 200, 800, 400))
    for i in range(3):
        assert tmp_ds.GetRasterBand(i + 1).ComputeStatistics(
            approx_ok=False
        ) == pytest.approx(
            vrt_ds.GetRasterBand(i + 1).ComputeStatistics(approx_ok=False), rel=1e-3
        )
    tmp_ds = None

    gdal.Unlink("/vsimem/small_world_pan_extended.vrt")


###############################################################################
# Test bands with different extents and positive geotransform[5] coefficient


def test_vrtpansharpen_band_with_different_extents_positive_yres():

    gdal.Warp(
        "/vsimem/small_world_pan_positive_yres.vrt",
        "tmp/small_world_pan.tif",
        options="-te -180 90 180 -90 -ts 800 400",
    )
    gdal.Warp(
        "/vsimem/small_world_ms_positive_yres.vrt",
        "data/small_world.tif",
        options="-te -180 90 180 -90 -ts 400 200",
    )

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename>/vsimem/small_world_pan_positive_yres.vrt</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>/vsimem/small_world_ms_positive_yres.vrt</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename>/vsimem/small_world_ms_positive_yres.vrt</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename>/vsimem/small_world_ms_positive_yres.vrt</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""

    vrt_ds = gdal.Open(xml)

    gdal.Translate(
        "/vsimem/small_world_pan_positive_yres_extended.vrt",
        "/vsimem/small_world_pan_positive_yres.vrt",
        options="-srcwin -100 -200 950 700",
    )

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename>/vsimem/small_world_pan_positive_yres_extended.vrt</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>/vsimem/small_world_ms_positive_yres.vrt</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename>/vsimem/small_world_ms_positive_yres.vrt</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename>/vsimem/small_world_ms_positive_yres.vrt</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""

    vrt_extended_ds = gdal.Open(xml)
    assert struct.unpack("B" * 3, vrt_extended_ds.ReadRaster(0, 0, 1, 1)) == (0, 0, 0)
    assert struct.unpack(
        "B" * 3, vrt_extended_ds.ReadRaster(vrt_extended_ds.RasterXSize - 1, 0, 1, 1)
    ) == (0, 0, 0)
    assert struct.unpack(
        "B" * 3, vrt_extended_ds.ReadRaster(0, vrt_extended_ds.RasterYSize - 1, 1, 1)
    ) == (0, 0, 0)
    assert struct.unpack(
        "B" * 3,
        vrt_extended_ds.ReadRaster(
            vrt_extended_ds.RasterXSize - 1, vrt_extended_ds.RasterYSize - 1, 1, 1
        ),
    ) == (0, 0, 0)
    assert struct.unpack(
        "B" * 3,
        vrt_extended_ds.ReadRaster(
            vrt_extended_ds.RasterXSize // 2, vrt_extended_ds.RasterYSize // 2, 1, 1
        ),
    ) != (0, 0, 0)

    # Check that the intersecting parts of the nominal and the extended
    # pansharpened datasets have very similar content (will be slightly
    # due to interpolation differences near the edges)
    tmp_ds = gdal.GetDriverByName("MEM").Create("", 800, 400, 3)
    tmp_ds.WriteRaster(0, 0, 800, 400, vrt_extended_ds.ReadRaster(100, 200, 800, 400))
    for i in range(3):
        assert tmp_ds.GetRasterBand(i + 1).ComputeStatistics(
            approx_ok=False
        ) == pytest.approx(
            vrt_ds.GetRasterBand(i + 1).ComputeStatistics(approx_ok=False), rel=1e-3
        )
    tmp_ds = None

    gdal.Unlink("/vsimem/small_world_ms_positive_yres.vrt")
    gdal.Unlink("/vsimem/small_world_pan_positive_yres_extended.vrt")
    gdal.Unlink("/vsimem/small_world_pan_positive_yres.vrt")


###############################################################################
# Test SerializeToXML()


def test_vrtpansharpen_8():

    xml = """<VRTDataset subClass="VRTPansharpenedDataset">
    <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2">
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
    </VRTRasterBand>
    <NoData>123</NoData>
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename relativeToVRT="1">small_world_pan.tif</SourceFilename>
        </PanchroBand>
        <SpectralBand dstBand="3">
                <SourceFilename>data/small_world.tif</SourceFilename>
        </SpectralBand>
        <SpectralBand dstBand="1">
                <SourceFilename>data/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>"""
    open("tmp/vrtpansharpen_8.vrt", "wt").write(xml)

    ds = gdal.Open("tmp/vrtpansharpen_8.vrt", gdal.GA_Update)
    expected_cs1 = ds.GetRasterBand(1).Checksum()
    expected_cs2 = ds.GetRasterBand(2).Checksum()
    expected_cs3 = ds.GetRasterBand(3).Checksum()
    # Force update
    ds.SetMetadata(ds.GetMetadata())
    ds = None

    ds = gdal.Open("tmp/vrtpansharpen_8.vrt")
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    ds = None

    gdal.Unlink("tmp/vrtpansharpen_8.vrt")

    assert cs1 == expected_cs1 and cs2 == expected_cs2 and cs3 == expected_cs3


###############################################################################
# Test NoData support


def test_vrtpansharpen_9():

    # Explicit nodata
    vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <NoData>0</NoData>
        <PanchroBand>
                <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>""")
    assert vrt_ds is not None
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    expected_cs_list = (
        [7056, 11779, 9026],
        [7052, 11770, 9018],  # s390x
        [7067, 11745, 8992],  # Intel(R) oneAPI DPC++/C++ Compiler 2022.1.0
    )
    assert cs in expected_cs_list

    # Implicit nodata
    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/small_world_pan_nodata.tif", 800, 400
    )
    src_ds = gdal.Open("tmp/small_world_pan.tif")
    ds.SetGeoTransform(src_ds.GetGeoTransform())
    ds.GetRasterBand(1).SetNoDataValue(0)
    ds.WriteRaster(0, 0, 800, 400, src_ds.ReadRaster())
    ds = None

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/small_world_nodata.tif", 400, 200, 3
    )
    src_ds = gdal.Open("data/small_world.tif")
    ds.SetGeoTransform(src_ds.GetGeoTransform())
    ds.GetRasterBand(1).SetNoDataValue(0)
    ds.GetRasterBand(2).SetNoDataValue(0)
    ds.GetRasterBand(3).SetNoDataValue(0)
    ds.WriteRaster(0, 0, 400, 200, src_ds.ReadRaster())
    ds = None

    vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
    <PansharpeningOptions>
        <PanchroBand>
                <SourceFilename>/vsimem/small_world_pan_nodata.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </PanchroBand>
        <SpectralBand dstBand="1">
                <SourceFilename>/vsimem/small_world_nodata.tif</SourceFilename>
                <SourceBand>1</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="2">
                <SourceFilename>/vsimem/small_world_nodata.tif</SourceFilename>
                <SourceBand>2</SourceBand>
        </SpectralBand>
        <SpectralBand dstBand="3">
                <SourceFilename>/vsimem/small_world_nodata.tif</SourceFilename>
                <SourceBand>3</SourceBand>
        </SpectralBand>
    </PansharpeningOptions>
</VRTDataset>""")
    assert vrt_ds is not None
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    assert cs in expected_cs_list

    gdal.Unlink("/vsimem/small_world_pan_nodata.tif")
    gdal.Unlink("/vsimem/small_world_nodata.tif")


###############################################################################
# Test UInt16 optimizations


def test_vrtpansharpen_10():

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/pan.tif", 1023, 1023, 1, gdal.GDT_UInt16
    )
    ds.SetGeoTransform([0, 1.0 / 1023, 0, 0, 0, 1.0 / 1023])
    ds.GetRasterBand(1).Fill(1000)
    ds = None
    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/ms.tif", 256, 256, 4, gdal.GDT_UInt16
    )
    ds.SetGeoTransform([0, 1.0 / 256, 0, 0, 0, 1.0 / 256])
    for i in range(4):
        ds.GetRasterBand(i + 1).Fill(1000)
    ds = None

    # 4 bands
    vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <NumThreads>ALL_CPUS</NumThreads>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">/vsimem/pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="4">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>4</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""")
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    assert cs == [62009, 62009, 62009, 62009]

    # Actually go through the optimized impl
    data = vrt_ds.ReadRaster()
    # And check
    data_int32 = vrt_ds.ReadRaster(buf_type=gdal.GDT_Int32)
    tmp_ds = gdal.GetDriverByName("MEM").Create(
        "", vrt_ds.RasterXSize, vrt_ds.RasterYSize, vrt_ds.RasterCount, gdal.GDT_Int32
    )
    tmp_ds.WriteRaster(0, 0, vrt_ds.RasterXSize, vrt_ds.RasterYSize, data_int32)
    ref_data = tmp_ds.ReadRaster(buf_type=gdal.GDT_UInt16)
    assert data == ref_data

    # 4 bands -> 3 bands
    vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <NumThreads>ALL_CPUS</NumThreads>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">/vsimem/pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
            <SpectralBand>
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>4</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""")
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    assert cs == [62009, 62009, 62009]

    # Actually go through the optimized impl
    data = vrt_ds.ReadRaster()
    # And check
    data_int32 = vrt_ds.ReadRaster(buf_type=gdal.GDT_Int32)
    tmp_ds = gdal.GetDriverByName("MEM").Create(
        "", vrt_ds.RasterXSize, vrt_ds.RasterYSize, vrt_ds.RasterCount, gdal.GDT_Int32
    )
    tmp_ds.WriteRaster(0, 0, vrt_ds.RasterXSize, vrt_ds.RasterYSize, data_int32)
    ref_data = tmp_ds.ReadRaster(buf_type=gdal.GDT_UInt16)
    assert data == ref_data

    # 3 bands
    vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <NumThreads>ALL_CPUS</NumThreads>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">/vsimem/pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""")
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    assert cs == [62009, 62009, 62009]

    # Actually go through the optimized impl
    data = vrt_ds.ReadRaster()
    # And check
    data_int32 = vrt_ds.ReadRaster(buf_type=gdal.GDT_Int32)
    tmp_ds = gdal.GetDriverByName("MEM").Create(
        "", vrt_ds.RasterXSize, vrt_ds.RasterYSize, vrt_ds.RasterCount, gdal.GDT_Int32
    )
    tmp_ds.WriteRaster(0, 0, vrt_ds.RasterXSize, vrt_ds.RasterYSize, data_int32)
    ref_data = tmp_ds.ReadRaster(buf_type=gdal.GDT_UInt16)
    assert data == ref_data


###############################################################################
# Test gdal.CreatePansharpenedVRT()


@gdaltest.disable_exceptions()
def test_vrtpansharpen_11():

    pan_ds = gdal.Open("tmp/small_world_pan.tif")
    ms_ds = gdal.Open("data/small_world.tif")

    vrt_ds = gdal.CreatePansharpenedVRT(
        """<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <SpectralBand dstBand="1">
            </SpectralBand>
            <SpectralBand dstBand="2">
            </SpectralBand>
            <SpectralBand dstBand="3">
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""",
        pan_ds.GetRasterBand(1),
        [ms_ds.GetRasterBand(i + 1) for i in range(3)],
    )
    assert pan_ds.GetRefCount() == 2
    assert ms_ds.GetRefCount() == 4

    pan_mem_ds = gdal.GetDriverByName("MEM").CreateCopy("", pan_ds)
    ms_mem_ds = gdal.GetDriverByName("MEM").CreateCopy("", ms_ds)

    # Check that dropping our ref to the input datasets works
    del pan_ds
    del ms_ds

    assert vrt_ds is not None
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    expected_cs = (
        [4735, 10000, 9742],
        [4731, 9991, 9734],
        [4726, 10004, 9727],  # ICC 2004.0.2 in -O3
    )
    assert cs in expected_cs

    # Also test with completely anonymous datasets

    vrt_ds = gdal.CreatePansharpenedVRT(
        """<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <SpectralBand dstBand="1">
            </SpectralBand>
            <SpectralBand dstBand="2">
            </SpectralBand>
            <SpectralBand dstBand="3">
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""",
        pan_mem_ds.GetRasterBand(1),
        [ms_mem_ds.GetRasterBand(i + 1) for i in range(3)],
    )
    assert vrt_ds is not None
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]
    assert cs in expected_cs
    vrt_ds = None

    # Check that wrapping with VRT works (when gt are not compatible)
    pan_mem_ds = gdal.GetDriverByName("MEM").Create("", 20, 40, 1)
    ms_mem_ds = gdal.GetDriverByName("MEM").Create("", 15, 30, 3)
    pan_mem_ds.SetGeoTransform([120, 1, 0, 80, 0, -1])
    ms_mem_ds.SetGeoTransform([100, 2, 0, 100, 0, -2])

    vrt_ds = gdal.CreatePansharpenedVRT(
        """<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <SpectralBand dstBand="1">
            </SpectralBand>
            <SpectralBand dstBand="2">
            </SpectralBand>
            <SpectralBand dstBand="3">
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""",
        pan_mem_ds.GetRasterBand(1),
        [ms_mem_ds.GetRasterBand(i + 1) for i in range(3)],
    )
    assert (
        vrt_ds.GetGeoTransform() == (100.0, 1.0, 0.0, 100.0, 0.0, -1.0)
        and vrt_ds.RasterXSize == 40
        and vrt_ds.RasterYSize == 60
    )
    vrt_ds = None

    # Test error cases as well
    with gdal.quiet_errors():
        vrt_ds = gdal.CreatePansharpenedVRT(
            """<invalid_xml""",
            pan_mem_ds.GetRasterBand(1),
            [ms_mem_ds.GetRasterBand(i + 1) for i in range(3)],
        )
    assert vrt_ds is None

    # Not enough bands
    with gdal.quiet_errors():
        vrt_ds = gdal.CreatePansharpenedVRT(
            """<VRTDataset subClass="VRTPansharpenedDataset">
            <PansharpeningOptions>
                <SpectralBand dstBand="1">
                </SpectralBand>
                <SpectralBand dstBand="2">
                </SpectralBand>
            </PansharpeningOptions>
        </VRTDataset>""",
            pan_mem_ds.GetRasterBand(1),
            [ms_mem_ds.GetRasterBand(i + 1) for i in range(3)],
        )
    assert vrt_ds is None

    # Too many bands
    with gdal.quiet_errors():
        vrt_ds = gdal.CreatePansharpenedVRT(
            """<VRTDataset subClass="VRTPansharpenedDataset">
            <PansharpeningOptions>
                <SpectralBand dstBand="1">
                </SpectralBand>
                <SpectralBand dstBand="2">
                </SpectralBand>
                <SpectralBand dstBand="3">
                </SpectralBand>
                <SpectralBand dstBand="4">
                </SpectralBand>
            </PansharpeningOptions>
        </VRTDataset>""",
            pan_mem_ds.GetRasterBand(1),
            [ms_mem_ds.GetRasterBand(i + 1) for i in range(3)],
        )
    assert vrt_ds is None


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/2328


def test_vrtpansharpen_nodata_multiple_spectral_bands():

    gdal.Translate("/vsimem/b1.tif", "data/small_world.tif")
    gdal.Translate("/vsimem/b2.tif", "data/small_world.tif")

    vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
  <PansharpeningOptions>
      <NoData>0</NoData>
    <PanchroBand>
      <SourceFilename>data/small_world.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </PanchroBand>
    <SpectralBand dstBand="1">
      <SourceFilename>/vsimem/b1.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SpectralBand>
    <SpectralBand dstBand="2">
      <SourceFilename>/vsimem/b2.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SpectralBand>
  </PansharpeningOptions>
</VRTDataset>""")
    assert vrt_ds

    gdal.Unlink("/vsimem/b1.tif")
    gdal.Unlink("/vsimem/b2.tif")


###############################################################################
# Test fix for https://github.com/OSGeo/gdal/issues/3189
# that is when the spectral bands have no nodata value, but we have one
# declared in PansharpeningOptions, and when the VRTPansharpenedDataset
# exposes overviews


def test_vrtpansharpen_nodata_overviews():

    ds = gdal.Translate("/vsimem/pan.tif", "data/byte.tif")
    ds.BuildOverviews("NEAR", [2])
    ds = None

    ds = gdal.Translate("/vsimem/ms.tif", "data/byte.tif")
    ds.BuildOverviews("NEAR", [2])
    ds = None

    vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
  <PansharpeningOptions>
      <NoData>0</NoData>
    <PanchroBand>
      <SourceFilename>/vsimem/pan.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </PanchroBand>
    <SpectralBand dstBand="1">
      <SourceFilename>/vsimem/ms.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SpectralBand>
  </PansharpeningOptions>
</VRTDataset>""")
    assert vrt_ds
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert vrt_ds.GetRasterBand(1).GetOverview(0).GetNoDataValue() == 0
    vrt_ds = None

    gdal.Unlink("/vsimem/pan.tif")
    gdal.Unlink("/vsimem/ms.tif")


###############################################################################
# Test input multispectral bands not in order 1,2,... and NoData as PansharpeningOptions


def test_vrtpansharpen_out_of_order_input_bands_and_nodata():

    src_ds = gdal.Open("data/small_world.tif")
    src_data = src_ds.GetRasterBand(1).ReadRaster()
    gt = src_ds.GetGeoTransform()
    wkt = src_ds.GetProjectionRef()
    src_ds = None
    pan_ds = gdal.GetDriverByName("MEM").Create("", 800, 400)
    gt = [gt[i] for i in range(len(gt))]
    gt[1] *= 0.5
    gt[5] *= 0.5
    pan_ds.SetGeoTransform(gt)
    pan_ds.SetProjection(wkt)
    pan_ds.GetRasterBand(1).WriteRaster(0, 0, 800, 400, src_data, 400, 200)

    ms_ds = gdal.Open("data/small_world.tif")

    vrt_ds = gdal.CreatePansharpenedVRT(
        """<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <AlgorithmOptions>
                <Weights>0.5,0.5</Weights>
            </AlgorithmOptions>
            <NoData>0</NoData>
            <SpectralBand dstBand="1">
            </SpectralBand>
            <SpectralBand dstBand="2">
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""",
        pan_ds.GetRasterBand(1),
        [ms_ds.GetRasterBand(i + 1) for i in range(2)],
    )
    assert vrt_ds is not None
    cs = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]

    # Switches the input multispectral bands
    vrt_ds = gdal.CreatePansharpenedVRT(
        """<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <AlgorithmOptions>
                <Weights>0.5,0.5</Weights>
            </AlgorithmOptions>
            <NoData>0</NoData>
            <SpectralBand dstBand="1">
            </SpectralBand>
            <SpectralBand dstBand="2">
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""",
        pan_ds.GetRasterBand(1),
        [ms_ds.GetRasterBand(2 - i) for i in range(2)],
    )
    assert vrt_ds is not None
    cs2 = [vrt_ds.GetRasterBand(i + 1).Checksum() for i in range(vrt_ds.RasterCount)]

    assert cs2 == cs[::-1]


###############################################################################
# Test open options for input bands


def test_vrtpansharpen_open_options_input_bands():
    def my_handler(typ, errno, msg):
        msgs.append(msg)

    msgs = []
    with gdaltest.error_handler(my_handler):
        gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <OpenOptions>
                       <OOI key="NUM_THREADS">foo</OOI>
                     </OpenOptions>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""")
        # Not the prettiest way to check that open options are used, but that does the job...
        assert "small_world_pan.tif: Invalid value for NUM_THREADS: foo" in msgs

    msgs = []
    with gdaltest.error_handler(my_handler):
        gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">tmp/small_world_pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <OpenOptions>
                       <OOI key="NUM_THREADS">foo</OOI>
                     </OpenOptions>
                     <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                    <SourceFilename relativeToVRT="1">data/small_world.tif</SourceFilename>
                    <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""")
        # Not the prettiest way to check that open options are used, but that does the job...
        assert "small_world.tif: Invalid value for NUM_THREADS: foo" in msgs


###############################################################################
# Test fix for #11889


def test_vrtpansharpen_slightly_different_extent(tmp_vsimem):

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/pan.tif", 1000, 1000, 1, gdal.GDT_UInt8
    )
    ds.SetGeoTransform([0, 1.0 / 1000, 0, 0, 0, -1.0 / 1000])
    ds.GetRasterBand(1).Fill(30)
    ds = None
    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/ms.tif", 500, 500, 2, gdal.GDT_UInt8
    )
    ds.SetGeoTransform(
        [0, 1.0 / 500 - 1.0 / 300000, 0, 0, 0, -(1.0 / 500 - 1.0 / 300000)]
    )
    ds.GetRasterBand(1).Fill(10)
    ds.GetRasterBand(2).Fill(20)
    ds = None

    vrt_ds = gdal.Open("""<VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <NumThreads>ALL_CPUS</NumThreads>
            <PanchroBand>
                    <SourceFilename relativeToVRT="1">/vsimem/pan.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                    <SourceFilename relativeToVRT="1">/vsimem/ms.tif</SourceFilename>
                    <SourceBand>2</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>""")
    mm = [
        vrt_ds.GetRasterBand(i + 1).ComputeRasterMinMax()
        for i in range(vrt_ds.RasterCount)
    ]
    assert mm == [(20.0, 20.0), (40.0, 40.0)]
