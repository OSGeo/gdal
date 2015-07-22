#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test VRTPansharpenedDataset support.
# Author:   Even Rouault <even.rouault at spatialys.com>
# 
###############################################################################
# Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

import sys
import shutil
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Error cases

def vrtpansharpen_1():
    
    src_ds = gdal.Open('data/small_world.tif')
    src_data = src_ds.GetRasterBand(1).ReadRaster()
    gt = src_ds.GetGeoTransform()
    wkt = src_ds.GetProjectionRef()
    src_ds = None
    pan_ds = gdal.GetDriverByName('GTiff').Create('tmp/small_world_pan.tif', 800, 400)
    gt = [ gt[i] for i in range(len(gt)) ]
    gt[1] *= 0.5
    gt[5] *= 0.5
    pan_ds.SetGeoTransform(gt)
    pan_ds.SetProjection(wkt)
    pan_ds.GetRasterBand(1).WriteRaster(0,0,800,400,src_data,400,200)
    pan_ds = None
    
    # Missing PansharpeningOptions
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
    <VRTRasterBand dataType="Byte" band="1" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Red</ColorInterp>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Green</ColorInterp>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
        <ColorInterp>Blue</ColorInterp>
    </VRTRasterBand>
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # PanchroBand missing
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # PanchroBand.SourceFilename missing
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid dataset name
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Inconsistant declared VRT dimensions with panchro dataset
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="1800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # VRTRasterBand of unrecognised subclass 'blabla'
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Algorithm unsupported_alg unsupported
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # 10 invalid band of tmp/small_world_pan.tif
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # SpectralBand.dstBand = '-1' invalid
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'


    # SpectralBand.SourceFilename missing
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Invalid dataset name
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # 10 invalid band of data/small_world.tif
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Another spectral band is already mapped to output band 1
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # No spectral band defined
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Hole in SpectralBand.dstBand numbering
    gdal.PushErrorHandler()
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
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Band 4 of type VRTPansharpenedRasterBand, but no corresponding SpectralBand
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # SpectralBand.dstBand = '3' invalid
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # 2 weights defined, but 3 input spectral bands
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Dimensions of input spectral band 1 different from first spectral band
    gdal.PushErrorHandler()
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
    gdal.PopErrorHandler()
    if vrt_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Just warnings
    # Warning 1: Pan dataset and data/byte.tif do not seem to have same projection. Results might be incorrect
    # Georeferencing of top-left corner of pan dataset and data/byte.tif do not match
    # Georeferencing of bottom-right corner of pan dataset and data/byte.tif do not match
    gdal.ErrorReset()
    gdal.PushErrorHandler()
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
    gdal.PopErrorHandler()
    if vrt_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    # Just warnings
    # No spectral band is mapped to an output band
    # No output pansharpened band defined
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    gdal.PopErrorHandler()
    if vrt_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    # Unsupported
    gdal.PushErrorHandler()
    ret = vrt_ds.AddBand(gdal.GDT_Byte) 
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Nominal cases

def vrtpansharpen_2():

    shutil.copy('data/small_world.tif', 'tmp/small_world.tif')

    # Super verbose case
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    if vrt_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrt_ds.GetFileList() != [ 'tmp/small_world_pan.tif', 'data/small_world.tif' ]:
        gdaltest.post_reason('fail')
        print(vrt_ds.GetFileList())
        return 'fail'
    if vrt_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    cs = [ vrt_ds.GetRasterBand(i+1).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [4735, 10000, 9742]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    if vrt_ds.GetRasterBand(1).GetOverviewCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrt_ds.GetRasterBand(1).GetOverview(-1) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrt_ds.GetRasterBand(1).GetOverview(0) is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check VRTPansharpenedDataset::IRasterIO() in non-resampling case
    data = vrt_ds.ReadRaster()
    tmp_ds = gdal.GetDriverByName('MEM').Create('',800,400,3)
    tmp_ds.WriteRaster(0,0,800,400,data)
    cs = [ tmp_ds.GetRasterBand(i+1).Checksum() for i in range(tmp_ds.RasterCount) ]
    if cs != [4735, 10000, 9742]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    # Check VRTPansharpenedDataset::IRasterIO() in resampling case
    data = vrt_ds.ReadRaster(0,0,800,400,400,200)
    ref_data = tmp_ds.ReadRaster(0,0,800,400,400,200)
    if data != ref_data:
        gdaltest.post_reason('fail')
        return 'fail'

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
    if vrt_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    cs = [ vrt_ds.GetRasterBand(i+1).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [4735, 10000, 9742]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'


    # Expose pan band too
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    if vrt_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('GTiff').CreateCopy('out1.tif', vrt_ds)
    cs = [ vrt_ds.GetRasterBand(i+1).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [50261, 4735, 10000, 9742]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    # Same, but everything scambled, and with spectral bands not in the same dataset
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
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
</VRTDataset>""")
    if vrt_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.GetDriverByName('GTiff').CreateCopy('out2.tif', vrt_ds)
    cs = [ vrt_ds.GetRasterBand(i+1).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [50261, 4735, 10000, 9742]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    return 'success'

###############################################################################
# Test with overviews

def vrtpansharpen_3():

    ds = gdal.Open('tmp/small_world_pan.tif')
    ds.BuildOverviews('CUBIC', [2])
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
    if vrt_ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrt_ds.GetRasterBand(1).GetOverview(0) is None:
        gdaltest.post_reason('fail')
        return 'fail'
    cs = [ vrt_ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [7123,7445,5025]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    # Check VRTPansharpenedDataset::IRasterIO() in resampling case with overviews
    data = vrt_ds.ReadRaster(0,0,800,400,400,200)
    
    data2 = vrt_ds.GetRasterBand(1).ReadRaster(0,0,800,400,400,200)
    data2 += vrt_ds.GetRasterBand(2).ReadRaster(0,0,800,400,400,200)
    data2 += vrt_ds.GetRasterBand(3).ReadRaster(0,0,800,400,400,200)
    
    if data != data2:
        gdaltest.post_reason('fail')
        return 'fail'
    
    tmp_ds = gdal.GetDriverByName('MEM').Create('',400,200,3)
    tmp_ds.WriteRaster(0,0,400,200,data)
    cs = [ tmp_ds.GetRasterBand(i+1).Checksum() for i in range(tmp_ds.RasterCount) ]
    if cs != [7123,7445,5025]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    vrt_ds = None

    ds = gdal.Open('tmp/small_world.tif')
    ds.BuildOverviews('CUBIC', [2])
    ds = None

    # Test when both Pan and spectral bands have overviews
    vrt_ds = gdal.Open(xml)
    if vrt_ds.GetRasterBand(1).GetOverviewCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if vrt_ds.GetRasterBand(1).GetOverview(0) is None:
        gdaltest.post_reason('fail')
        return 'fail'
    cs = [ vrt_ds.GetRasterBand(i+1).GetOverview(0).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [18033, 18395, 16824]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    vrt_ds = None

    return 'success'

###############################################################################
# Test RasterIO() with various buffer datatypes

def vrtpansharpen_4():

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
    for dt in [ gdal.GDT_Int16, gdal.GDT_UInt16, gdal.GDT_Int32, gdal.GDT_UInt32, gdal.GDT_Float32, gdal.GDT_Float64, gdal.GDT_CFloat64 ]:
        data = vrt_ds.GetRasterBand(1).ReadRaster(buf_type = dt)
        tmp_ds = gdal.GetDriverByName('MEM').Create('',800,400,1,dt)
        tmp_ds.WriteRaster(0,0,800,400,data)
        cs = tmp_ds.GetRasterBand(1).Checksum()
        if dt == gdal.GDT_CFloat64:
            expected_cs = 4724
        else:
            expected_cs = 4735
        if cs != expected_cs:
            gdaltest.post_reason('fail')
            print(gdal.GetDataTypeName(dt))
            print(cs)
            return 'fail'

    return 'success'

###############################################################################
# Test RasterIO() with various band datatypes

def vrtpansharpen_5():

    for dt in [ gdal.GDT_Int16, gdal.GDT_UInt16, gdal.GDT_Int32, gdal.GDT_UInt32, gdal.GDT_Float32, gdal.GDT_Float64, gdal.GDT_CFloat64 ]:
        
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
</VRTDataset>""" % (gdal.GetDataTypeName(dt), gdal.GetDataTypeName(dt), gdal.GetDataTypeName(dt))
        
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
</VRTDataset>""" % (gdal.GetDataTypeName(dt), spectral_xml, spectral_xml, spectral_xml)

        vrt_ds = gdal.Open(xml)
        data = vrt_ds.GetRasterBand(1).ReadRaster(buf_type = gdal.GDT_Byte)
        tmp_ds = gdal.GetDriverByName('MEM').Create('',800,400,1)
        tmp_ds.WriteRaster(0,0,800,400,data)
        cs = tmp_ds.GetRasterBand(1).Checksum()
        if dt == gdal.GDT_UInt16:
            if cs != 4553:
                gdaltest.post_reason('fail')
                print(gdal.GetDataTypeName(dt))
                print(cs)
                return 'fail'
        else:
            if cs != 4450:
                gdaltest.post_reason('fail')
                print(gdal.GetDataTypeName(dt))
                print(cs)
                return 'fail'


    return 'success'

###############################################################################
# Test BitDepth limitations

def vrtpansharpen_6():
    
    try:
        from osgeo import gdalnumeric
        gdalnumeric.zeros
        import numpy
    except:
        return 'skip'

    # i = 0: VRT has <BitDepth>7</BitDepth>
    # i = 1: bands have NBITS=7 and VRT <BitDepth>7</BitDepth>
    # i = 2: bands have NBITS=7
    for dt in [ gdal.GDT_Byte, gdal.GDT_UInt16 ]:
        if dt == gdal.GDT_Byte:
            nbits = 7
        elif dt == gdal.GDT_UInt16:
            nbits = 12
        else:
            nbits = 17
        for i in range(3):
            if i > 0:
                options = ['NBITS=%d' % nbits]
            else:
                options = []
            mem_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/ms.tif', 4, 1, 1, dt, options = options)
            ar = numpy.array([[80,125,125,80]])
            if dt == gdal.GDT_UInt16:
                ar = ar << (12-7)
            elif dt == gdal.GDT_UInt32:
                ar = ar << (17-7)
            mem_ds.GetRasterBand(1).WriteArray(ar)
            mem_ds = None

            mem_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/pan.tif', 8, 2, 1, dt, options = options)
            ar = numpy.array([[ 76, 89, 115, 127, 127, 115, 89, 76],
                              [ 76, 89, 115, 127, 127, 115, 89, 76]])
            if dt == gdal.GDT_UInt16:
                ar = ar << (12-7)
            elif dt == gdal.GDT_UInt32:
                ar = ar << (17-7)
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
            if vrt_ds.GetRasterBand(1).GetMetadataItem('NBITS', 'IMAGE_STRUCTURE') != str(nbits):
                gdaltest.post_reason('fail')
                return 'fail'

            ar = vrt_ds.GetRasterBand(1).ReadAsArray()
            if dt == gdal.GDT_Byte:
                expected_ar = [95, 111, 127, 127, 127, 127, 111, 95]
            elif dt == gdal.GDT_UInt16:
                expected_ar = [3040, 3560, 4095, 4095, 4095, 4095, 3560, 3040]
            else:
                expected_ar = [97280, 113920, 131071, 131071, 131071, 131071, 113920, 97280]

            if list(ar[0]) != expected_ar:
                gdaltest.post_reason('fail')
                print(gdal.GetDataTypeName(dt))
                print(i)
                print(list(ar[0]))
                return 'fail'
            vrt_ds = None

            gdal.Unlink('/vsimem/ms.tif')
            gdal.Unlink('/vsimem/pan.tif')
        
    return 'success'

###############################################################################
# Test bands with different extents

def vrtpansharpen_7():
    
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrtpansharpen_7_pan.tif', 20, 40 )
    ds.SetGeoTransform([120,1,0,80,0,-1])
    ds = None
    
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrtpansharpen_7_ms.tif', 15, 30 )
    ds.SetGeoTransform([100,2,0,100,0,-2])
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
    if ds.GetGeoTransform() != (100.0, 1.0, 0.0, 100.0, 0.0, -1.0) or ds.RasterXSize != 40 or ds.RasterYSize != 60:
        gdaltest.post_reason('fail')
        print(ds.GetGeoTransform())
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'
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
    if ds.GetGeoTransform() != (100.0, 1.0, 0.0, 100.0, 0.0, -1.0) or ds.RasterXSize != 40 or ds.RasterYSize != 60:
        gdaltest.post_reason('fail')
        print(ds.GetGeoTransform())
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'
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
    gdal.PushErrorHandler()
    ds = gdal.Open(xml)
    gdal.PopErrorHandler()
    if ds.GetGeoTransform() != (100.0, 1.0, 0.0, 100.0, 0.0, -1.0) or ds.RasterXSize != 40 or ds.RasterYSize != 60:
        gdaltest.post_reason('fail')
        print(ds.GetGeoTransform())
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
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
    if ds.GetGeoTransform() != (120.0, 1.0, 0.0, 80.0, 0.0, -1.0) or ds.RasterXSize != 10 or ds.RasterYSize != 40:
        gdaltest.post_reason('fail')
        print(ds.GetGeoTransform())
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'
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
    gdal.PushErrorHandler()
    ds = gdal.Open(xml)
    gdal.PopErrorHandler()
    if ds.GetGeoTransform() != (120.0, 1.0, 0.0, 80.0, 0.0, -1.0) or ds.RasterXSize != 20 or ds.RasterYSize != 40:
        gdaltest.post_reason('fail')
        print(ds.GetGeoTransform())
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
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
    if ds.GetGeoTransform() != (120.0, 1.0, 0.0, 80.0, 0.0, -1.0) or ds.RasterXSize != 20 or ds.RasterYSize != 40:
        gdaltest.post_reason('fail')
        print(ds.GetGeoTransform())
        print(ds.RasterXSize)
        print(ds.RasterYSize)
        return 'fail'
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    
    # Empty intersection
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrtpansharpen_7_ms.tif', 15, 30 )
    ds.SetGeoTransform([-100,2,0,-100,0,-2])
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
    gdal.PushErrorHandler()
    ds = gdal.Open(xml)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrtpansharpen_7_pan.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrtpansharpen_7_ms.tif')
        
    return 'success'

###############################################################################
# Test SerializeToXML()

def vrtpansharpen_8():

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
    open('tmp/vrtpansharpen_8.vrt', 'wt').write(xml)

    ds = gdal.Open('tmp/vrtpansharpen_8.vrt', gdal.GA_Update)
    expected_cs1 = ds.GetRasterBand(1).Checksum()
    expected_cs2 = ds.GetRasterBand(2).Checksum()
    expected_cs3 = ds.GetRasterBand(3).Checksum()
    # Force update
    ds.SetMetadata(ds.GetMetadata())
    ds = None

    ds = gdal.Open('tmp/vrtpansharpen_8.vrt')
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    cs3 = ds.GetRasterBand(3).Checksum()
    ds = None

    gdal.Unlink('tmp/vrtpansharpen_8.vrt')
    
    if cs1 != expected_cs1 or cs2 != expected_cs2 or cs3 != expected_cs3:
        gdaltest.post_reason('fail')
        print(cs1)
        print(expected_cs1)
        print(cs2)
        print(expected_cs2)
        print(cs3)
        print(expected_cs3)
        return 'fail'
        
    return 'success'

###############################################################################
# Test NoData support

def vrtpansharpen_9():

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
    if vrt_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    cs = [ vrt_ds.GetRasterBand(i+1).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [4179, 8767, 52257]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'

    # Implicit nodata
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/small_world_pan_nodata.tif', 800, 400)
    ds.GetRasterBand(1).SetNoDataValue(0)
    ds.WriteRaster(0,0,800,400,gdal.Open('tmp/small_world_pan.tif').ReadRaster())
    ds = None
    
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/small_world_nodata.tif', 400, 200,3)
    ds.GetRasterBand(1).SetNoDataValue(0)
    ds.GetRasterBand(2).SetNoDataValue(0)
    ds.GetRasterBand(3).SetNoDataValue(0)
    ds.WriteRaster(0,0,400,200,gdal.Open('data/small_world.tif').ReadRaster())
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
    if vrt_ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    cs = [ vrt_ds.GetRasterBand(i+1).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [4179, 8767, 52257]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
        
    return 'success'

###############################################################################
# Test UInt16 optimizations

def vrtpansharpen_10():
    
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/pan.tif',1023,1023,1,gdal.GDT_UInt16)
    ds.GetRasterBand(1).Fill(1000)
    ds = None
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/ms.tif',256,256,4,gdal.GDT_UInt16)
    for i in range(4):
        ds.GetRasterBand(i+1).Fill(1000)
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
    cs = [ vrt_ds.GetRasterBand(i+1).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [62009, 62009, 62009, 62009]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    
    # Actually go through the optimized impl
    data = vrt_ds.ReadRaster()
    # And check
    data_int32 = vrt_ds.ReadRaster(buf_type = gdal.GDT_Int32)
    tmp_ds = gdal.GetDriverByName('MEM').Create('', vrt_ds.RasterXSize, vrt_ds.RasterYSize, vrt_ds.RasterCount, gdal.GDT_Int32)
    tmp_ds.WriteRaster(0, 0, vrt_ds.RasterXSize, vrt_ds.RasterYSize, data_int32)
    ref_data = tmp_ds.ReadRaster(buf_type = gdal.GDT_UInt16)
    if data != ref_data:
        gdaltest.post_reason('fail')
        return 'fail'

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
    cs = [ vrt_ds.GetRasterBand(i+1).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [62009, 62009, 62009]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    
    # Actually go through the optimized impl
    data = vrt_ds.ReadRaster()
    # And check
    data_int32 = vrt_ds.ReadRaster(buf_type = gdal.GDT_Int32)
    tmp_ds = gdal.GetDriverByName('MEM').Create('', vrt_ds.RasterXSize, vrt_ds.RasterYSize, vrt_ds.RasterCount, gdal.GDT_Int32)
    tmp_ds.WriteRaster(0, 0, vrt_ds.RasterXSize, vrt_ds.RasterYSize, data_int32)
    ref_data = tmp_ds.ReadRaster(buf_type = gdal.GDT_UInt16)
    if data != ref_data:
        gdaltest.post_reason('fail')
        return 'fail'
    
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
    cs = [ vrt_ds.GetRasterBand(i+1).Checksum() for i in range(vrt_ds.RasterCount) ]
    if cs != [62009, 62009, 62009]:
        gdaltest.post_reason('fail')
        print(cs)
        return 'fail'
    
    # Actually go through the optimized impl
    data = vrt_ds.ReadRaster()
    # And check
    data_int32 = vrt_ds.ReadRaster(buf_type = gdal.GDT_Int32)
    tmp_ds = gdal.GetDriverByName('MEM').Create('', vrt_ds.RasterXSize, vrt_ds.RasterYSize, vrt_ds.RasterCount, gdal.GDT_Int32)
    tmp_ds.WriteRaster(0, 0, vrt_ds.RasterXSize, vrt_ds.RasterYSize, data_int32)
    ref_data = tmp_ds.ReadRaster(buf_type = gdal.GDT_UInt16)
    if data != ref_data:
        gdaltest.post_reason('fail')
        return 'fail'
    
    return 'success'

###############################################################################
# Cleanup

def vrtpansharpen_cleanup():
    
    gdal.GetDriverByName('GTiff').Delete('tmp/small_world_pan.tif')
    gdal.GetDriverByName('GTiff').Delete('tmp/small_world.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/pan.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/ms.tif')

    return 'success'


gdaltest_list = [
    vrtpansharpen_1,
    vrtpansharpen_2,
    vrtpansharpen_3,
    vrtpansharpen_4,
    vrtpansharpen_5,
    vrtpansharpen_6,
    vrtpansharpen_7,
    vrtpansharpen_8,
    vrtpansharpen_9,
    vrtpansharpen_10,
    vrtpansharpen_cleanup,
]

#gdaltest_list= [ vrtpansharpen_1, vrtpansharpen_8 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'vrtpansharpen' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

