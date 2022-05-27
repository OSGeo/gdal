#!/usr/bin/env python3
###############################################################################
#  $Id$
#
# Purpose:  Module for retiling (merging) tiles and building tiled pyramids
# Author:   Christian Meuller, christian.mueller@nvoe.at
# UseDirForEachRow support by Chris Giesey & Elijah Robison
#
###############################################################################
# Copyright (c) 2007, Christian Mueller
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
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
from __future__ import print_function
import os
import sys
import shutil

from osgeo import gdal
from osgeo import ogr
from osgeo import osr

progress = gdal.TermProgress_nocb


class AffineTransformDecorator(object):
    """ A class providing some useful methods for affine Transformations """

    def __init__(self, transform):
        self.lrx = None
        self.lry = None
        self.geotransform = transform
        self.scaleX = self.geotransform[1]
        self.scaleY = self.geotransform[5]
        if self.scaleY > 0:
            self.scaleY *= -1
        self.ulx = self.geotransform[0]
        self.uly = self.geotransform[3]

    def pointsFor(self, width, height):
        xlist = []
        ylist = []
        w = self.scaleX * width
        h = self.scaleY * height

        xlist.append(self.ulx)
        ylist.append(self.uly)
        xlist.append(self.ulx + w)
        ylist.append(self.uly)
        xlist.append(self.ulx + w)
        ylist.append(self.uly + h)
        xlist.append(self.ulx)
        ylist.append(self.uly + h)
        return [xlist, ylist]


class DataSetCache(object):
    """ A class for caching source tiles """

    def __init__(self):
        self.cacheSize = 8
        self.queue = []
        self.dict = {}

    def get(self, name):

        if name in self.dict:
            return self.dict[name]
        result = gdal.Open(name)
        if result is None:
            print("Error opening: %s" % NameError)
            return 1
        if len(self.queue) == self.cacheSize:
            toRemove = self.queue.pop(0)
            del self.dict[toRemove]
        self.queue.append(name)
        self.dict[name] = result
        return result

    def __del__(self):
        for dataset in self.dict.values():
            del dataset
        del self.queue
        del self.dict


class tile_info(object):
    """ A class holding info how to tile """

    def __init__(self, xsize, ysize, tileWidth, tileHeight, overlap):
        self.width = xsize
        self.height = ysize
        self.tileWidth = tileWidth
        self.tileHeight = tileHeight
        self.countTilesX = 1
        if xsize > tileWidth:
            self.countTilesX += int((xsize - tileWidth + (tileWidth - overlap) - 1) / (tileWidth - overlap))
        self.countTilesY = 1
        if ysize > tileHeight:
            self.countTilesY += int((ysize - tileHeight + (tileHeight - overlap) - 1) / (tileHeight - overlap))
        self.overlap = overlap

    def report(self):
        print('tileWidth:   %d' % self.tileWidth)
        print('tileHeight:  %d' % self.tileHeight)
        print('countTilesX: %d' % self.countTilesX)
        print('countTilesY: %d' % self.countTilesY)
        print('overlap:     %d' % self.overlap)


class mosaic_info(object):
    """A class holding information about a GDAL file or a GDAL fileset"""

    def __init__(self, filename, inputDS):
        """
        Initialize mosaic_info from filename

        filename -- Name of file to read.

        """
        self.TempDriver = gdal.GetDriverByName("MEM")
        self.filename = filename
        self.cache = DataSetCache()
        self.ogrTileIndexDS = inputDS

        self.ogrTileIndexDS.GetLayer().ResetReading()
        feature = self.ogrTileIndexDS.GetLayer().GetNextFeature()
        imgLocation = feature.GetField(0)

        fhInputTile = self.cache.get(imgLocation)

        self.bands = fhInputTile.RasterCount
        self.band_type = fhInputTile.GetRasterBand(1).DataType
        self.projection = fhInputTile.GetProjection()
        self.nodata = fhInputTile.GetRasterBand(1).GetNoDataValue()

        dec = AffineTransformDecorator(fhInputTile.GetGeoTransform())
        self.scaleX = dec.scaleX
        self.scaleY = dec.scaleY
        ct = fhInputTile.GetRasterBand(1).GetRasterColorTable()
        if ct is not None:
            self.ct = ct.Clone()
        else:
            self.ct = None
        self.ci = [0] * self.bands
        for iband in range(self.bands):
            self.ci[iband] = fhInputTile.GetRasterBand(iband + 1).GetRasterColorInterpretation()

        extent = self.ogrTileIndexDS.GetLayer().GetExtent()
        self.ulx = extent[0]
        self.uly = extent[3]
        self.lrx = extent[1]
        self.lry = extent[2]

        self.xsize = int(round((self.lrx - self.ulx) / self.scaleX))
        self.ysize = abs(int(round((self.uly - self.lry) / self.scaleY)))

    def __del__(self):
        del self.cache
        del self.ogrTileIndexDS

    def getDataSet(self, minx, miny, maxx, maxy):

        self.ogrTileIndexDS.GetLayer().ResetReading()
        self.ogrTileIndexDS.GetLayer().SetSpatialFilterRect(minx, miny, maxx, maxy)
        features = []
        envelope = None
        while True:
            feature = self.ogrTileIndexDS.GetLayer().GetNextFeature()
            if feature is None:
                break
            features.append(feature)
            if envelope is None:
                envelope = feature.GetGeometryRef().GetEnvelope()
            else:
                featureEnv = feature.GetGeometryRef().GetEnvelope()
                envelope = (min(featureEnv[0], envelope[0]), max(featureEnv[1], envelope[1]),
                            min(featureEnv[2], envelope[2]), max(featureEnv[3], envelope[3]))

        if envelope is None:
            return None

        # enlarge to query rect if necessary
        envelope = (min(minx, envelope[0]), max(maxx, envelope[1]),
                    min(miny, envelope[2]), max(maxy, envelope[3]))

        self.ogrTileIndexDS.GetLayer().SetSpatialFilter(None)

        # merge tiles

        resultSizeX = int((maxx - minx) / self.scaleX + 0.5)
        resultSizeY = int((miny - maxy) / self.scaleY + 0.5)

        resultDS = self.TempDriver.Create("TEMP", resultSizeX, resultSizeY, self.bands, self.band_type, [])
        resultDS.SetGeoTransform([minx, self.scaleX, 0, maxy, 0, self.scaleY])

        for bandNr in range(1, self.bands + 1):
            t_band = resultDS.GetRasterBand(bandNr)
            if self.nodata is not None:
                t_band.Fill(self.nodata)
                t_band.SetNoDataValue(self.nodata)

        for feature in features:
            featureName = feature.GetField(0)
            sourceDS = self.cache.get(featureName)
            dec = AffineTransformDecorator(sourceDS.GetGeoTransform())

            dec.lrx = dec.ulx + sourceDS.RasterXSize * dec.scaleX
            dec.lry = dec.uly + sourceDS.RasterYSize * dec.scaleY

            # Find the intersection region
            tgw_ulx = max(dec.ulx, minx)
            tgw_lrx = min(dec.lrx, maxx)
            if self.scaleY < 0:
                tgw_uly = min(dec.uly, maxy)
                tgw_lry = max(dec.lry, miny)
            else:
                tgw_uly = max(dec.uly, maxy)
                tgw_lry = min(dec.lry, miny)

            # Compute source window in pixel coordinates.
            sw_xoff = int((tgw_ulx - dec.ulx) / dec.scaleX + 0.5)
            sw_yoff = int((tgw_uly - dec.uly) / dec.scaleY + 0.5)
            sw_xsize = min(sourceDS.RasterXSize, int((tgw_lrx - dec.ulx) / dec.scaleX + 0.5)) - sw_xoff
            sw_ysize = min(sourceDS.RasterYSize, int((tgw_lry - dec.uly) / dec.scaleY + 0.5)) - sw_yoff
            if sw_xsize <= 0 or sw_ysize <= 0:
                continue

            # Compute target window in pixel coordinates
            tw_xoff = int((tgw_ulx - minx) / self.scaleX + 0.5)
            tw_yoff = int((tgw_uly - maxy) / self.scaleY + 0.5)
            tw_xsize = min(resultDS.RasterXSize, int((tgw_lrx - minx) / self.scaleX + 0.5)) - tw_xoff
            tw_ysize = min(resultDS.RasterYSize, int((tgw_lry - maxy) / self.scaleY + 0.5)) - tw_yoff
            if tw_xsize <= 0 or tw_ysize <= 0:
                continue

            assert tw_xoff >= 0
            assert tw_yoff >= 0
            assert sw_xoff >= 0
            assert sw_yoff >= 0

            for bandNr in range(1, self.bands + 1):
                s_band = sourceDS.GetRasterBand(bandNr)
                t_band = resultDS.GetRasterBand(bandNr)
                if self.ct is not None:
                    t_band.SetRasterColorTable(self.ct)
                t_band.SetRasterColorInterpretation(self.ci[bandNr - 1])

                data = s_band.ReadRaster(sw_xoff, sw_yoff, sw_xsize, sw_ysize, tw_xsize, tw_ysize, self.band_type)
                if data is None:
                    print(gdal.GetLastErrorMsg())

                t_band.WriteRaster(tw_xoff, tw_yoff, tw_xsize, tw_ysize, data)

        return resultDS

    def closeDataSet(self, memDS):
        del memDS
        # self.TempDriver.Delete("TEMP")

    def report(self):
        print('Filename: ' + self.filename)
        print('File Size: %dx%dx%d'
              % (self.xsize, self.ysize, self.bands))
        print('Pixel Size: %f x %f'
              % (self.scaleX, self.scaleY))
        print('UL:(%f,%f)   LR:(%f,%f)'
              % (self.ulx, self.uly, self.lrx, self.lry))


def getTileIndexFromFiles(g):
    if g.Verbose:
        print("Building internal Index for %d tile(s) ..." % len(g.Names), end=" ")

    ogrTileIndexDS = createTileIndex(g.Verbose,  "TileIndex", g.TileIndexFieldName, None, g.TileIndexDriverTyp)
    for inputTile in g.Names:

        fhInputTile = gdal.Open(inputTile)
        if fhInputTile is None:
            return None

        dec = AffineTransformDecorator(fhInputTile.GetGeoTransform())
        points = dec.pointsFor(fhInputTile.RasterXSize, fhInputTile.RasterYSize)

        addFeature(g.TileIndexFieldName, ogrTileIndexDS, inputTile, points[0], points[1])
        del fhInputTile

    if g.Verbose:
        print("finished")
    # ogrTileIndexDS.GetLayer().SyncToDisk()
    return ogrTileIndexDS


def getTargetDir(g, level=-1):
    if level == -1:
        return g.TargetDir
    return g.TargetDir + str(level) + os.sep


def tileImage(g, minfo, ti):
    """

    Tile image in mosaicinfo minfo  based on tileinfo ti

    returns list of created tiles

    """

    g.LastRowIndx = -1
    OGRDS = createTileIndex(g.Verbose,  "TileResult_0", g.TileIndexFieldName, g.Source_SRS, g.TileIndexDriverTyp)

    yRange = list(range(1, ti.countTilesY + 1))
    xRange = list(range(1, ti.countTilesX + 1))

    if not g.Quiet and not g.Verbose:
        progress(0.0)
        processed = 0
        total = len(xRange) * len(yRange)

    for yIndex in yRange:
        for xIndex in xRange:
            offsetY = (yIndex - 1) * (ti.tileHeight - ti.overlap)
            offsetX = (xIndex - 1) * (ti.tileWidth - ti.overlap)
            height = ti.tileHeight
            width = ti.tileWidth
            if g.UseDirForEachRow:
                tilename = getTileName(g, minfo, ti, xIndex, yIndex, 0)
            else:
                tilename = getTileName(g, minfo, ti, xIndex, yIndex)

            if offsetX + width > ti.width:
                width = ti.width - offsetX
            if offsetY + height > ti.height:
                height = ti.height - offsetY

            feature_only = g.Resume and os.path.exists(tilename)
            createTile(g, minfo, offsetX, offsetY, width, height, tilename, OGRDS, feature_only)

            if not g.Quiet and not g.Verbose:
                processed += 1
                progress(processed / float(total))

    if g.TileIndexName is not None:
        if g.UseDirForEachRow and not g.PyramidOnly:
            shapeName = getTargetDir(g, 0) + g.TileIndexName
        else:
            shapeName = getTargetDir(g) + g.TileIndexName
        copyTileIndexToDisk(g, OGRDS, shapeName)

    if g.CsvFileName is not None:
        if g.UseDirForEachRow and not g.PyramidOnly:
            csvName = getTargetDir(g, 0) + g.CsvFileName
        else:
            csvName = getTargetDir(g) + g.CsvFileName
        copyTileIndexToCSV(g, OGRDS, csvName)

    return OGRDS


def copyTileIndexToDisk(g, OGRDS, fileName):
    SHAPEDS = createTileIndex(g.Verbose,  fileName, g.TileIndexFieldName, OGRDS.GetLayer().GetSpatialRef(), "ESRI Shapefile")
    OGRDS.GetLayer().ResetReading()
    while True:
        feature = OGRDS.GetLayer().GetNextFeature()
        if feature is None:
            break
        newFeature = feature.Clone()
        basename = os.path.basename(feature.GetField(0))
        if g.UseDirForEachRow:
            t = os.path.split(os.path.dirname(feature.GetField(0)))
            basename = t[1] + "/" + basename
        newFeature.SetField(0, basename)
        SHAPEDS.GetLayer().CreateFeature(newFeature)
    closeTileIndex(SHAPEDS)


def copyTileIndexToCSV(g, OGRDS, fileName):
    csvfile = open(fileName, 'w')
    OGRDS.GetLayer().ResetReading()
    while True:
        feature = OGRDS.GetLayer().GetNextFeature()
        if feature is None:
            break
        basename = os.path.basename(feature.GetField(0))
        if g.UseDirForEachRow:
            t = os.path.split(os.path.dirname(feature.GetField(0)))
            basename = t[1] + "/" + basename
        csvfile.write(basename)
        geom = feature.GetGeometryRef()
        coords = geom.GetEnvelope()

        for coord in coords:
            csvfile.write(g.CsvDelimiter)
            csvfile.write("%f" % coord)
        csvfile.write("\n")

    csvfile.close()


def createPyramidTile(g, levelMosaicInfo, offsetX, offsetY, width, height, tileName, OGRDS, feature_only):

    temp_tilename = tileName + '.tmp'

    sx = levelMosaicInfo.scaleX * 2
    sy = levelMosaicInfo.scaleY * 2

    dec = AffineTransformDecorator([levelMosaicInfo.ulx + offsetX * sx, sx, 0,
                                    levelMosaicInfo.uly + offsetY * sy, 0, sy])

    if OGRDS is not None:
        points = dec.pointsFor(width, height)
        addFeature(g.TileIndexFieldName, OGRDS, tileName, points[0], points[1])

    if feature_only:
        return

    s_fh = levelMosaicInfo.getDataSet(dec.ulx, dec.uly + height * dec.scaleY,
                                      dec.ulx + width * dec.scaleX, dec.uly)
    if s_fh is None:
        return

    if g.BandType is None:
        bt = levelMosaicInfo.band_type
    else:
        bt = g.BandType

    geotransform = [dec.ulx, dec.scaleX, 0, dec.uly, 0, dec.scaleY]

    bands = levelMosaicInfo.bands

    if g.MemDriver is None:
        t_fh = g.Driver.Create(temp_tilename, width, height, bands, bt, g.CreateOptions)
    else:
        t_fh = g.MemDriver.Create('', width, height, bands, bt)

    if t_fh is None:
        print('Creation failed, terminating gdal_tile.')
        return 1

    t_fh.SetGeoTransform(geotransform)
    t_fh.SetProjection(levelMosaicInfo.projection)
    for band in range(1, bands + 1):
        t_band = t_fh.GetRasterBand(band)
        if levelMosaicInfo.ct is not None:
            t_band.SetRasterColorTable(levelMosaicInfo.ct)
        t_band.SetRasterColorInterpretation(levelMosaicInfo.ci[band - 1])

    if levelMosaicInfo.nodata is not None:
        for band in range(1, bands + 1):
            t_band = t_fh.GetRasterBand(band)
            t_band.Fill(levelMosaicInfo.nodata)
            t_band.SetNoDataValue(levelMosaicInfo.nodata)

    res = gdal.ReprojectImage(s_fh, t_fh, None, None, g.ResamplingMethod)
    if res != 0:
        print("Reprojection failed for %s, error %d" % (temp_tilename, res))
        return 1

    levelMosaicInfo.closeDataSet(s_fh)

    if g.MemDriver is None:
        t_fh.FlushCache()
    else:
        tt_fh = g.Driver.CreateCopy(temp_tilename, t_fh, 0, g.CreateOptions)
        tt_fh.FlushCache()
        tt_fh = None

    t_fh = None

    if os.path.exists(tileName):
        os.remove(tileName)
    shutil.move(temp_tilename, tileName)

    if g.Verbose:
        print(tileName + " : " + str(offsetX) + "|" + str(offsetY) + "-->" + str(width) + "-" + str(height))


def createTile(g, minfo, offsetX, offsetY, width, height, tilename, OGRDS, feature_only):
    """

    Create tile

    """
    temp_tilename = tilename + '.tmp'

    if g.BandType is None:
        bt = minfo.band_type
    else:
        bt = g.BandType

    dec = AffineTransformDecorator([minfo.ulx, minfo.scaleX, 0, minfo.uly, 0, minfo.scaleY])

    geotransform = [dec.ulx + offsetX * dec.scaleX, dec.scaleX, 0,
                    dec.uly + offsetY * dec.scaleY, 0, dec.scaleY]

    if OGRDS is not None:
        dec2 = AffineTransformDecorator(geotransform)
        points = dec2.pointsFor(width, height)
        addFeature(g.TileIndexFieldName, OGRDS, tilename, points[0], points[1])

    if feature_only:
        return

    s_fh = minfo.getDataSet(dec.ulx + offsetX * dec.scaleX, dec.uly + offsetY * dec.scaleY + height * dec.scaleY,
                            dec.ulx + offsetX * dec.scaleX + width * dec.scaleX,
                            dec.uly + offsetY * dec.scaleY)
    if s_fh is None:
        return

    bands = minfo.bands

    if g.MemDriver is None:
        t_fh = g.Driver.Create(temp_tilename, width, height, bands, bt, g.CreateOptions)
    else:
        t_fh = g.MemDriver.Create('', width, height, bands, bt)

    if t_fh is None:
        print('Creation failed, terminating gdal_tile.')
        return 1

    t_fh.SetGeoTransform(geotransform)
    if g.Source_SRS is not None:
        t_fh.SetProjection(g.Source_SRS.ExportToWkt())

    readX = min(s_fh.RasterXSize, width)
    readY = min(s_fh.RasterYSize, height)
    for band in range(1, bands + 1):
        s_band = s_fh.GetRasterBand(band)
        t_band = t_fh.GetRasterBand(band)
        if minfo.ct is not None:
            t_band.SetRasterColorTable(minfo.ct)
        if minfo.nodata is not None:
            t_band.Fill(minfo.nodata)
            t_band.SetNoDataValue(minfo.nodata)

        data = s_band.ReadRaster(0, 0, readX, readY, readX, readY, t_band.DataType)
        t_band.WriteRaster(0, 0, readX, readY, data, readX, readY, t_band.DataType)

    minfo.closeDataSet(s_fh)

    if g.MemDriver is None:
        t_fh.FlushCache()
    else:
        tt_fh = g.Driver.CreateCopy(temp_tilename, t_fh, 0, g.CreateOptions)
        tt_fh.FlushCache()
        tt_fh = None

    t_fh = None

    if os.path.exists(tilename):
        os.remove(tilename)
    shutil.move(temp_tilename, tilename)

    if g.Verbose:
        print(tilename + " : " + str(offsetX) + "|" + str(offsetY) + "-->" + str(width) + "-" + str(height))


def createTileIndex(Verbose, dsName, fieldName, srs, driverName):
    OGRDriver = ogr.GetDriverByName(driverName)
    if OGRDriver is None:
        print('ESRI Shapefile driver not found')
        return 1

    OGRDataSource = OGRDriver.Open(dsName)
    if OGRDataSource is not None:
        OGRDataSource.Destroy()
        OGRDriver.DeleteDataSource(dsName)
        if Verbose:
            print('truncating index ' + dsName)

    OGRDataSource = OGRDriver.CreateDataSource(dsName)
    if OGRDataSource is None:
        print('Could not open datasource ' + dsName)
        return 1

    OGRLayer = OGRDataSource.CreateLayer("index", srs, ogr.wkbPolygon)
    if OGRLayer is None:
        print('Could not create Layer')
        return 1

    OGRFieldDefn = ogr.FieldDefn(fieldName, ogr.OFTString)
    if OGRFieldDefn is None:
        print('Could not create FieldDefn for ' + fieldName)
        return 1

    OGRFieldDefn.SetWidth(256)
    if OGRLayer.CreateField(OGRFieldDefn) != 0:
        print('Could not create Field for ' + fieldName)
        return 1

    return OGRDataSource


def addFeature(TileIndexFieldName, OGRDataSource, location, xlist, ylist):
    OGRLayer = OGRDataSource.GetLayer()
    OGRFeature = ogr.Feature(OGRLayer.GetLayerDefn())
    if OGRFeature is None:
        print('Could not create Feature')
        return 1

    OGRFeature.SetField(TileIndexFieldName, location)
    wkt = 'POLYGON ((%f %f,%f %f,%f %f,%f %f,%f %f ))' % (xlist[0], ylist[0],
                                                          xlist[1], ylist[1], xlist[2], ylist[2], xlist[3], ylist[3], xlist[0], ylist[0])
    OGRGeometry = ogr.CreateGeometryFromWkt(wkt, OGRLayer.GetSpatialRef())
    if OGRGeometry is None:
        print('Could not create Geometry')
        return 1

    OGRFeature.SetGeometryDirectly(OGRGeometry)

    OGRLayer.CreateFeature(OGRFeature)
    OGRFeature.Destroy()


def closeTileIndex(OGRDataSource):
    OGRDataSource.Destroy()


def buildPyramid(g, minfo, createdTileIndexDS, tileWidth, tileHeight, overlap):
    inputDS = createdTileIndexDS
    for level in range(1, g.Levels + 1):
        g.LastRowIndx = -1
        levelMosaicInfo = mosaic_info(minfo.filename, inputDS)
        levelOutputTileInfo = tile_info(int(levelMosaicInfo.xsize / 2), int(levelMosaicInfo.ysize / 2), tileWidth, tileHeight, overlap)
        inputDS = buildPyramidLevel(g, levelMosaicInfo, levelOutputTileInfo, level)


def buildPyramidLevel(g, levelMosaicInfo, levelOutputTileInfo, level):
    yRange = list(range(1, levelOutputTileInfo.countTilesY + 1))
    xRange = list(range(1, levelOutputTileInfo.countTilesX + 1))

    OGRDS = createTileIndex(g.Verbose,  "TileResult_" + str(level), g.TileIndexFieldName, g.Source_SRS, g.TileIndexDriverTyp)

    for yIndex in yRange:
        for xIndex in xRange:
            offsetY = (yIndex - 1) * (levelOutputTileInfo.tileHeight - levelOutputTileInfo.overlap)
            offsetX = (xIndex - 1) * (levelOutputTileInfo.tileWidth - levelOutputTileInfo.overlap)
            height = levelOutputTileInfo.tileHeight
            width = levelOutputTileInfo.tileWidth

            if offsetX + width > levelOutputTileInfo.width:
                width = levelOutputTileInfo.width - offsetX
            if offsetY + height > levelOutputTileInfo.height:
                height = levelOutputTileInfo.height - offsetY

            tilename = getTileName(g, levelMosaicInfo, levelOutputTileInfo, xIndex, yIndex, level)

            feature_only = g.Resume and os.path.exists(tilename)
            createPyramidTile(g, levelMosaicInfo, offsetX, offsetY, width, height, tilename, OGRDS, feature_only)

    if g.TileIndexName is not None:
        shapeName = getTargetDir(g, level) + g.TileIndexName
        copyTileIndexToDisk(g, OGRDS, shapeName)

    if g.CsvFileName is not None:
        csvName = getTargetDir(g, level) + g.CsvFileName
        copyTileIndexToCSV(g, OGRDS, csvName)

    return OGRDS


def getTileName(g, minfo, ti, xIndex, yIndex, level=-1):
    """
    creates the tile file name
    """
    maxim = ti.countTilesX
    if ti.countTilesY > maxim:
        maxim = ti.countTilesY
    countDigits = len(str(maxim))
    parts = os.path.splitext(os.path.basename(minfo.filename))
    if parts[0][0] == "@":  # remove possible leading "@"
        parts = (parts[0][1:len(parts[0])], parts[1])

    yIndex_str = ("%0" + str(countDigits) + "i") % (yIndex,)
    xIndex_str = ("%0" + str(countDigits) + "i") % (xIndex,)

    if g.UseDirForEachRow:
        frmt = getTargetDir(g, level) + str(yIndex) + os.sep + parts[0] + "_" + yIndex_str + "_" + xIndex_str
        # See if there was a switch in the row, if so then create new dir for row.
        if g.LastRowIndx < yIndex:
            g.LastRowIndx = yIndex
            if not os.path.exists(getTargetDir(g, level) + str(yIndex)):
                os.mkdir(getTargetDir(g, level) + str(yIndex))
    else:
        frmt = getTargetDir(g, level) + parts[0] + "_" + yIndex_str + "_" + xIndex_str
    # Check for the extension that should be used.
    if g.Extension is None:
        frmt += parts[1]
    else:
        frmt += "." + g.Extension
    return frmt


def UsageFormat():
    print('Valid formats:')
    count = gdal.GetDriverCount()
    for index in range(count):
        driver = gdal.GetDriver(index)
        print(driver.ShortName)
    return 1

# =============================================================================


def Usage():
    print('Usage: gdal_retile.py ')
    print('        [-v] [-q] [-co NAME=VALUE]* [-of out_format]')
    print('        [-ps pixelWidth pixelHeight]')
    print('        [-overlap val_in_pixel]')
    print('        [-ot  {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/')
    print('               CInt16/CInt32/CFloat32/CFloat64}]')
    print('        [ -tileIndex tileIndexName [-tileIndexField fieldName]]')
    print('        [ -csv fileName [-csvDelim delimiter]]')
    print('        [-s_srs srs_def]  [-pyramidOnly] -levels numberoflevels')
    print('        [-r {near/bilinear/cubic/cubicspline/lanczos}]')
    print('        [-useDirForEachRow] [-resume]')
    print('        -targetDir TileDirectory input_files')
    return 2


def main(args=None, g=None):
    if g is None:
        g = RetileGlobals()

    if args is None:
        args = sys.argv
    argv = gdal.GeneralCmdLineProcessor(args)
    if argv is None:
        return 1

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-of' or arg == '-f':
            i += 1
            g.Format = argv[i]
        elif arg == '-ot':
            i += 1
            g.BandType = gdal.GetDataTypeByName(argv[i])
            if g.BandType == gdal.GDT_Unknown:
                print('Unknown GDAL data type: %s' % argv[i])
                return 1
        elif arg == '-co':
            i += 1
            g.CreateOptions.append(argv[i])

        elif arg == '-v':
            g.Verbose = True
        elif arg == '-q':
            g.Quiet = True

        elif arg == '-targetDir':
            i += 1
            g.TargetDir = argv[i]

            if not os.path.exists(g.TargetDir):
                print("TargetDir " + g.TargetDir + " does not exist")
                return 1
            if g.TargetDir[len(g.TargetDir) - 1:] != os.sep:
                g.TargetDir = g.TargetDir + os.sep

        elif arg == '-ps':
            i += 1
            g.TileWidth = int(argv[i])
            i += 1
            g.TileHeight = int(argv[i])

        elif arg == '-overlap':
            i += 1
            g.Overlap = int(argv[i])

        elif arg == '-r':
            i += 1
            ResamplingMethodString = argv[i]
            if ResamplingMethodString == "near":
                g.ResamplingMethod = gdal.GRA_NearestNeighbour
            elif ResamplingMethodString == "bilinear":
                g.ResamplingMethod = gdal.GRA_Bilinear
            elif ResamplingMethodString == "cubic":
                g.ResamplingMethod = gdal.GRA_Cubic
            elif ResamplingMethodString == "cubicspline":
                g.ResamplingMethod = gdal.GRA_CubicSpline
            elif ResamplingMethodString == "lanczos":
                g.ResamplingMethod = gdal.GRA_Lanczos
            else:
                print("Unknown resampling method: %s" % ResamplingMethodString)
                return 1
        elif arg == '-levels':
            i += 1
            g.Levels = int(argv[i])
            if g.Levels < 1:
                print("Invalid number of levels : %d" % g.Levels)
                return 1
        elif arg == '-s_srs':
            i += 1
            g.Source_SRS = osr.SpatialReference()
            if g.Source_SRS.SetFromUserInput(argv[i]) != 0:
                print('invalid -s_srs: ' + argv[i])
                return 1

        elif arg == "-pyramidOnly":
            g.PyramidOnly = True
        elif arg == '-tileIndex':
            i += 1
            g.TileIndexName = argv[i]
            parts = os.path.splitext(g.TileIndexName)
            if not parts[1]:
                g.TileIndexName += ".shp"

        elif arg == '-tileIndexField':
            i += 1
            g.TileIndexFieldName = argv[i]
        elif arg == '-csv':
            i += 1
            g.CsvFileName = argv[i]
            parts = os.path.splitext(g.CsvFileName)
            if not parts[1]:
                g.CsvFileName += ".csv"
        elif arg == '-csvDelim':
            i += 1
            g.CsvDelimiter = argv[i]
        elif arg == '-useDirForEachRow':
            g.UseDirForEachRow = True
        elif arg == "-resume":
            g.Resume = True
        elif arg[:1] == '-':
            print('Unrecognized command option: %s' % arg)
            return Usage()

        else:
            g.Names.append(arg)
        i += 1

    if not g.Names:
        print('No input files selected.')
        return Usage()

    if (g.TileWidth == 0 or g.TileHeight == 0):
        print("Invalid tile dimension %d,%d" % (g.TileWidth, g.TileHeight))
        return 1
    if (g.TileWidth - g.Overlap <= 0 or g.TileHeight - g.Overlap <= 0):
        print("Overlap too big w.r.t tile height/width")
        return 1

    if g.TargetDir is None:
        print("Missing Directory for Tiles -targetDir")
        return Usage()

    # create level 0 directory if needed
    if g.UseDirForEachRow and not g.PyramidOnly:
        leveldir = g.TargetDir + str(0) + os.sep
        if not os.path.exists(leveldir):
            os.mkdir(leveldir)

    if g.Levels > 0:  # prepare Dirs for pyramid
        startIndx = 1
        for levelIndx in range(startIndx, g.Levels + 1):
            leveldir = g.TargetDir + str(levelIndx) + os.sep
            if os.path.exists(leveldir):
                continue
            os.mkdir(leveldir)
            if not os.path.exists(leveldir):
                print("Cannot create level dir: %s" % leveldir)
                return 1
            if g.Verbose:
                print("Created level dir: %s" % leveldir)

    g.Driver = gdal.GetDriverByName(g.Format)
    if g.Driver is None:
        print('Format driver %s not found, pick a supported driver.' % g.Format)
        return UsageFormat()

    DriverMD = g.Driver.GetMetadata()
    g.Extension = DriverMD.get(gdal.DMD_EXTENSION)
    if 'DCAP_CREATE' not in DriverMD:
        g.MemDriver = gdal.GetDriverByName("MEM")

    tileIndexDS = getTileIndexFromFiles(g)
    if tileIndexDS is None:
        print("Error building tile index")
        return 1
    minfo = mosaic_info(g.Names[0], tileIndexDS)
    ti = tile_info(minfo.xsize, minfo.ysize, g.TileWidth, g.TileHeight, g.Overlap)

    if g.Source_SRS is None and minfo.projection:
        g.Source_SRS = osr.SpatialReference()
        if g.Source_SRS.SetFromUserInput(minfo.projection) != 0:
            print('invalid projection  ' + minfo.projection)
            return 1

    if g.Verbose:
        minfo.report()
        ti.report()

    if not g.PyramidOnly:
        dsCreatedTileIndex = tileImage(g, minfo, ti)
        tileIndexDS.Destroy()
    else:
        dsCreatedTileIndex = tileIndexDS

    if g.Levels > 0:
        buildPyramid(g, minfo, dsCreatedTileIndex, g.TileWidth, g.TileHeight, g.Overlap)

    if g.Verbose:
        print("FINISHED")
    return 0


class RetileGlobals():
    __slots__ = ['Verbose',
        'Quiet',
        'CreateOptions',
        'Names',
        'TileWidth',
        'TileHeight',
        'Overlap',
        'Format',
        'BandType',
        'Driver',
        'Extension',
        'MemDriver',
        'TileIndexFieldName',
        'TileIndexName',
        'TileIndexDriverTyp',
        'CsvDelimiter',
        'CsvFileName',
        'Source_SRS',
        'TargetDir',
        'ResamplingMethod',
        'Levels',
        'PyramidOnly',
        'LastRowIndx',
        'UseDirForEachRow',
        'Resume']

    def __init__(self):
        """ Only used for unit tests """
        self.Verbose = False
        self.Quiet = False
        self.CreateOptions = []
        self.Names = []
        self.TileWidth = 256
        self.TileHeight = 256
        self.Overlap = 0
        self.Format = 'GTiff'
        self.BandType = None
        self.Driver = None
        self.Extension = None
        self.MemDriver = None
        self.TileIndexFieldName = 'location'
        self.TileIndexName = None
        self.TileIndexDriverTyp = "Memory"
        self.CsvDelimiter = ";"
        self.CsvFileName = None

        self.Source_SRS = None
        self.TargetDir = None
        self.ResamplingMethod = gdal.GRA_NearestNeighbour
        self.Levels = 0
        self.PyramidOnly = False
        self.LastRowIndx = -1
        self.UseDirForEachRow = False
        self.Resume = False


if __name__ == '__main__':
    sys.exit(main(sys.argv))
