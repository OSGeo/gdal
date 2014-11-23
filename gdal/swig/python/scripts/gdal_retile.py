#!/usr/bin/env python
###############################################################################
#  $Id$
#
# Purpose:  Module for retiling (merging) tiles and building tiled pyramids
# Author:   Christian Meuller, christian.mueller@nvoe.at
# UseDirForEachRow support by Chris Giesey & Elijah Robison
#
###############################################################################
# Copyright (c) 2007, Christian Mueller
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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


try:
    from osgeo import gdal
    from osgeo import ogr
    from osgeo import osr
    from osgeo.gdalconst import *
except:
    import gdal
    import ogr
    import osr
    from gdalconst import *

import sys
import os
import math

class AffineTransformDecorator:
    """ A class providing some useful methods for affine Transformations """
    def __init__(self, transform ):
        self.geotransform=transform
        self.scaleX=self.geotransform[1]
        self.scaleY=self.geotransform[5]
        if self.scaleY > 0:
            self.scaleY*=-1
        self.ulx = self.geotransform[0]
        self.uly = self.geotransform[3]

    def pointsFor(self,width,height):
        xlist=[]
        ylist=[]
        w=self.scaleX * width;
        h=self.scaleY * height;

        xlist.append(self.ulx)
        ylist.append(self.uly)
        xlist.append(self.ulx+w)
        ylist.append(self.uly)
        xlist.append(self.ulx+w)
        ylist.append(self.uly+h)
        xlist.append(self.ulx)
        ylist.append(self.uly+h)
        return [ xlist, ylist]


class DataSetCache:
    """ A class for caching source tiles """
    def __init__(self ):
        self.cacheSize=8
        self.queue=[]
        self.dict={}
    def get(self,name ):

        if name in self.dict:
            return self.dict[name]
        result = gdal.Open(name)
        if result is None:
            print("Error opening: %s" % NameError)
            sys.exit(1)
        if len(self.queue)==self.cacheSize:
            toRemove = self.queue.pop(0)
            del self.dict[toRemove]
        self.queue.append(name)
        self.dict[name]=result
        return result
    def __del__(self):
        for name, dataset in self.dict.items():
            del dataset
        del self.queue
        del self.dict



class tile_info:
    """ A class holding info how to tile """
    def __init__(self,xsize,ysize,tileWidth,tileHeight):
        self.tileWidth=tileWidth
        self.tileHeight=tileHeight
        self.countTilesX= int(xsize / tileWidth)
        self.countTilesY= int(ysize / tileHeight)
        self.lastTileWidth = int(xsize - self.countTilesX *  tileWidth)
        self.lastTileHeight = int(ysize - self.countTilesY *  tileHeight)

        if (self.lastTileWidth > 0 ):
            self.countTilesX=self.countTilesX+1
        else:
            self.lastTileWidth=tileWidth

        if (self.lastTileHeight > 0 ):
            self.countTilesY=self.countTilesY+1
        else:
            self.lastTileHeight=tileHeight



    def report( self ):
        print('tileWidth       %d' % self.tileWidth)
        print('tileHeight      %d' % self.tileHeight)
        print('countTilesX:    %d' % self.countTilesX)
        print('countTilesY:    %d' % self.countTilesY)
        print('lastTileWidth:  %d' % self.lastTileWidth)
        print('lastTileHeight: %d' % self.lastTileHeight)



class mosaic_info:
    """A class holding information about a GDAL file or a GDAL fileset"""

    def __init__(self, filename,inputDS ):
        """
        Initialize mosaic_info from filename

        filename -- Name of file to read.

        """
        self.TempDriver=gdal.GetDriverByName("MEM")
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


        dec = AffineTransformDecorator(fhInputTile.GetGeoTransform())
        self.scaleX=dec.scaleX
        self.scaleY=dec.scaleY
        ct = fhInputTile.GetRasterBand(1).GetRasterColorTable()
        if ct is not None:
           self.ct = ct.Clone()
        else:
           self.ct = None
        self.ci = [0] * self.bands
        for iband in range(self.bands):
            self.ci[iband] = fhInputTile.GetRasterBand(iband + 1).GetRasterColorInterpretation()

        extent = self.ogrTileIndexDS.GetLayer().GetExtent()
        self.ulx = extent[0];
        self.uly = extent[3]
        self.lrx = extent[1]
        self.lry = extent[2]

        self.xsize = int(round((self.lrx-self.ulx) / self.scaleX))
        self.ysize = abs(int(round((self.uly-self.lry) / self.scaleY)))


    def __del__(self):
        del self.cache
        del self.ogrTileIndexDS

    def getDataSet(self,minx,miny,maxx,maxy):

        self.ogrTileIndexDS.GetLayer().ResetReading()
        self.ogrTileIndexDS.GetLayer().SetSpatialFilterRect(minx,miny,maxx,maxy)
        features = []
        envelope = None
        while True:
            feature = self.ogrTileIndexDS.GetLayer().GetNextFeature();
            if feature is None:
                break
            features.append(feature)
            if envelope is None:
                envelope=feature.GetGeometryRef().GetEnvelope()
            else:
                featureEnv = feature.GetGeometryRef().GetEnvelope()
                envelope= ( min(featureEnv[0],envelope[0]),max(featureEnv[1],envelope[1]),
                            min(featureEnv[2],envelope[2]),max(featureEnv[3],envelope[3]))

        if envelope is None:
            return None

        #enlarge to query rect if necessary
        envelope= ( min(minx,envelope[0]),max(maxx,envelope[1]),
                    min(miny,envelope[2]),max(maxy,envelope[3]))


        self.ogrTileIndexDS.GetLayer().SetSpatialFilter(None)

         # merge tiles


        resultSizeX =int(math.ceil(((maxx-minx) / self.scaleX )))
        resultSizeY =int(math.ceil(((miny-maxy) / self.scaleY )))

        resultDS = self.TempDriver.Create( "TEMP", resultSizeX, resultSizeY, self.bands,self.band_type,[])
        resultDS.SetGeoTransform( [minx,self.scaleX,0,maxy,0,self.scaleY] )


        for feature in features:
            featureName =  feature.GetField(0)
            sourceDS=self.cache.get(featureName)
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
            sw_xoff = int((tgw_ulx - dec.ulx) / dec.scaleX)
            sw_yoff = int((tgw_uly - dec.uly) / dec.scaleY)
            sw_xsize = int((tgw_lrx - dec.ulx) / dec.scaleX + 0.5) - sw_xoff
            sw_ysize = int((tgw_lry - dec.uly) / dec.scaleY + 0.5) - sw_yoff
            if sw_xsize <= 0 or sw_ysize <= 0:
                continue

            # Compute target window in pixel coordinates
            tw_xoff = int((tgw_ulx - minx) / self.scaleX)
            tw_yoff = int((tgw_uly - maxy) / self.scaleY)
            tw_xsize = int((tgw_lrx - minx) / self.scaleX + 0.5) - tw_xoff
            tw_ysize = int((tgw_lry - maxy) / self.scaleY + 0.5) - tw_yoff
            if tw_xsize <= 0 or tw_ysize <= 0:
                continue

            assert tw_xoff >= 0
            assert tw_yoff >= 0
            assert sw_xoff >= 0
            assert sw_yoff >= 0
            
            for bandNr in range(1, self.bands + 1):
                s_band = sourceDS.GetRasterBand( bandNr )
                t_band = resultDS.GetRasterBand( bandNr )
                if self.ct is not None:
                    t_band.SetRasterColorTable(self.ct)
                t_band.SetRasterColorInterpretation(self.ci[bandNr-1])
                
                data = s_band.ReadRaster( sw_xoff, sw_yoff, sw_xsize, sw_ysize, tw_xsize, tw_ysize, self.band_type )
                if data is None:                    
                    print(gdal.GetLastErrorMsg())

                t_band.WriteRaster(tw_xoff, tw_yoff, tw_xsize, tw_ysize, data )

        return resultDS

    def closeDataSet(self, memDS):
        del memDS
        #self.TempDriver.Delete("TEMP")


    def report( self ):
        print('Filename: '+ self.filename)
        print('File Size: %dx%dx%d' \
              % (self.xsize, self.ysize, self.bands))
        print('Pixel Size: %f x %f' \
              % (self.scaleX,self.scaleY))
        print('UL:(%f,%f)   LR:(%f,%f)' \
              % (self.ulx,self.uly,self.lrx,self.lry))


def getTileIndexFromFiles( inputTiles, driverTyp):

    if Verbose:
        from sys import version_info
        if version_info >= (3,0,0):
            exec('print("Building internal Index for %d tile(s) ..." % len(inputTiles), end=" ")')
        else:
            exec('print "Building internal Index for %d tile(s) ..." % len(inputTiles), ')

    ogrTileIndexDS = createTileIndex("TileIndex",TileIndexFieldName,None,driverTyp);
    for inputTile in inputTiles:

        fhInputTile = gdal.Open(inputTile)
        if fhInputTile is None:
             return None

        dec = AffineTransformDecorator(fhInputTile.GetGeoTransform())
        points = dec.pointsFor(fhInputTile.RasterXSize, fhInputTile.RasterYSize)

        addFeature(ogrTileIndexDS,inputTile,points[0],points[1])
        del fhInputTile

    if Verbose:
        print("finished")
    #ogrTileIndexDS.GetLayer().SyncToDisk()
    return ogrTileIndexDS




def getTargetDir (level = -1):
    if level==-1:
        return TargetDir
    else:
        return TargetDir+str(level)+os.sep




def tileImage(minfo, ti ):
    """

    Tile image in mosaicinfo minfo  based on tileinfo ti

    returns list of created tiles

    """

    global LastRowIndx
    LastRowIndx=-1
    OGRDS=createTileIndex("TileResult_0", TileIndexFieldName, Source_SRS,TileIndexDriverTyp)


    yRange = list(range(1,ti.countTilesY+1))
    xRange = list(range(1,ti.countTilesX+1))

    for yIndex in yRange:
        for xIndex in xRange:
            offsetY=(yIndex-1)* ti.tileHeight
            offsetX=(xIndex-1)* ti.tileWidth
            if yIndex==ti.countTilesY:
                height=ti.lastTileHeight
            else:
                height=ti.tileHeight

            if xIndex==ti.countTilesX:
                width=ti.lastTileWidth
            else:
                width=ti.tileWidth
            if UseDirForEachRow :
                tilename=getTileName(minfo,ti, xIndex, yIndex,0)
            else:
                tilename=getTileName(minfo,ti, xIndex, yIndex)
            createTile(minfo, offsetX, offsetY, width, height,tilename,OGRDS)


    if TileIndexName is not None:
        if UseDirForEachRow and PyramidOnly == False:
            shapeName=getTargetDir(0)+TileIndexName
        else:
            shapeName=getTargetDir()+TileIndexName
        copyTileIndexToDisk(OGRDS,shapeName)

    if CsvFileName is not None:
        if UseDirForEachRow and PyramidOnly == False:
            csvName=getTargetDir(0)+CsvFileName
        else:
            csvName=getTargetDir()+CsvFileName
        copyTileIndexToCSV(OGRDS,csvName)


    return OGRDS

def copyTileIndexToDisk(OGRDS, fileName):
    SHAPEDS = createTileIndex(fileName, TileIndexFieldName, OGRDS.GetLayer().GetSpatialRef(), "ESRI Shapefile")
    OGRDS.GetLayer().ResetReading()
    while True:
      feature = OGRDS.GetLayer().GetNextFeature()
      if feature is None:
          break
      newFeature = feature.Clone()
      basename = os.path.basename(feature.GetField(0))
      if UseDirForEachRow :
          t = os.path.split(os.path.dirname(feature.GetField(0)))
          basename = t[1]+"/"+basename
      newFeature.SetField(0,basename)
      SHAPEDS.GetLayer().CreateFeature(newFeature)
    closeTileIndex(SHAPEDS)

def copyTileIndexToCSV(OGRDS, fileName):
    csvfile = open(fileName, 'w')
    OGRDS.GetLayer().ResetReading()
    while True:
      feature = OGRDS.GetLayer().GetNextFeature()
      if feature is None:
          break
      basename = os.path.basename(feature.GetField(0))
      if UseDirForEachRow :
          t = os.path.split(os.path.dirname(feature.GetField(0)))
          basename = t[1]+"/"+basename
      csvfile.write(basename);
      geom = feature.GetGeometryRef()
      coords = geom.GetEnvelope();

      for i in range(len(coords)):
          csvfile.write(CsvDelimiter)
          csvfile.write("%f" % coords[i])
      csvfile.write("\n");

    csvfile.close()



def createPyramidTile(levelMosaicInfo, offsetX, offsetY, width, height,tileName,OGRDS):

    sx= levelMosaicInfo.scaleX*2
    sy= levelMosaicInfo.scaleY*2

    dec = AffineTransformDecorator([levelMosaicInfo.ulx+offsetX*sx,sx,0,
                                    levelMosaicInfo.uly+offsetY*sy,0,sy])



    s_fh = levelMosaicInfo.getDataSet(dec.ulx,dec.uly+height*dec.scaleY,
                         dec.ulx+width*dec.scaleX,dec.uly)
    if s_fh is None:
        return


    if OGRDS is not None:
        points = dec.pointsFor(width, height)
        addFeature(OGRDS, tileName, points[0], points[1])


    if BandType is None:
        bt=levelMosaicInfo.band_type
    else:
        bt=BandType

    geotransform = [dec.ulx, dec.scaleX, 0,dec.uly,0,dec.scaleY]


    bands = levelMosaicInfo.bands

    if MemDriver is None:
        t_fh = Driver.Create( tileName, width, height, bands,bt,CreateOptions)
    else:
        t_fh = MemDriver.Create( tileName, width, height, bands,bt)

    if t_fh is None:
        print('Creation failed, terminating gdal_tile.')
        sys.exit( 1 )


    t_fh.SetGeoTransform( geotransform )
    t_fh.SetProjection( levelMosaicInfo.projection)
    for band in range(1,bands+1):
        t_band = t_fh.GetRasterBand( band )
        if levelMosaicInfo.ct is not None:
            t_band.SetRasterColorTable(levelMosaicInfo.ct)
        t_band.SetRasterColorInterpretation(levelMosaicInfo.ci[band-1])

    res = gdal.ReprojectImage(s_fh,t_fh,None,None,ResamplingMethod)
    if  res!=0:
        print("Reprojection failed for %s, error %d" % (tileName,res))
        sys.exit( 1 )


    levelMosaicInfo.closeDataSet(s_fh);

    if MemDriver is not None:
        tt_fh = Driver.CreateCopy( tileName, t_fh, 0, CreateOptions )

    if Verbose:
        print(tileName + " : " + str(offsetX)+"|"+str(offsetY)+"-->"+str(width)+"-"+str(height))




def createTile( minfo, offsetX,offsetY,width,height, tilename,OGRDS):
    """

    Create tile
    return name of created tile

    """

    if BandType is None:
        bt=minfo.band_type
    else:
        bt=BandType


    dec = AffineTransformDecorator([minfo.ulx,minfo.scaleX,0,minfo.uly,0,minfo.scaleY])


    s_fh = minfo.getDataSet(dec.ulx+offsetX*dec.scaleX,dec.uly+offsetY*dec.scaleY+height*dec.scaleY,
                         dec.ulx+offsetX*dec.scaleX+width*dec.scaleX,
                         dec.uly+offsetY*dec.scaleY)
    if s_fh is None:
        return;


    geotransform = [dec.ulx+offsetX*dec.scaleX, dec.scaleX, 0,
                    dec.uly+offsetY*dec.scaleY,  0,dec.scaleY]

    if OGRDS is not None:
        dec2 = AffineTransformDecorator(geotransform)
        points = dec2.pointsFor(width, height)
        addFeature(OGRDS, tilename, points[0], points[1])



    bands = minfo.bands

    if MemDriver is None:
        t_fh = Driver.Create( tilename, width, height, bands,bt,CreateOptions)
    else:
        t_fh = MemDriver.Create( tilename, width, height, bands,bt)

    if t_fh is None:
        print('Creation failed, terminating gdal_tile.')
        sys.exit( 1 )

    t_fh.SetGeoTransform( geotransform )
    if Source_SRS is not None:
        t_fh.SetProjection( Source_SRS.ExportToWkt())

    readX=min(s_fh.RasterXSize,width)
    readY=min(s_fh.RasterYSize,height)
    for band in range(1,bands+1):
        s_band = s_fh.GetRasterBand( band )
        t_band = t_fh.GetRasterBand( band )
        if minfo.ct is not None:
            t_band.SetRasterColorTable(minfo.ct)

#        data = s_band.ReadRaster( offsetX,offsetY,width,height,width,height, t_band.DataType )
        data = s_band.ReadRaster( 0,0,readX,readY,readX,readY,  t_band.DataType )
        t_band.WriteRaster( 0,0,readX,readY, data,readX,readY, t_band.DataType )

    minfo.closeDataSet(s_fh);

    if MemDriver is not None:
        tt_fh = Driver.CreateCopy( tilename, t_fh, 0, CreateOptions )

    if Verbose:
        print(tilename + " : " + str(offsetX)+"|"+str(offsetY)+"-->"+str(width)+"-"+str(height))



def createTileIndex(dsName,fieldName,srs,driverName):

    OGRDriver = ogr.GetDriverByName(driverName);
    if OGRDriver is None:
        print('ESRI Shapefile driver not found')
        sys.exit( 1 )

    OGRDataSource=OGRDriver.Open(dsName)
    if OGRDataSource is not None:
        OGRDataSource.Destroy()
        OGRDriver.DeleteDataSource(dsName)
        if Verbose:
            print('truncating index '+ dsName)

    OGRDataSource=OGRDriver.CreateDataSource(dsName)
    if OGRDataSource is None:
        print('Could not open datasource '+dsName)
        sys.exit( 1 )

    OGRLayer = OGRDataSource.CreateLayer("index", srs, ogr.wkbPolygon)
    if OGRLayer is None:
        print('Could not create Layer')
        sys.exit( 1 )

    OGRFieldDefn = ogr.FieldDefn(fieldName,ogr.OFTString)
    if OGRFieldDefn is None:
        print('Could not create FieldDefn for '+fieldName)
        sys.exit( 1 )

    OGRFieldDefn.SetWidth(256)
    if OGRLayer.CreateField(OGRFieldDefn) != 0:
        print('Could not create Field for '+fieldName)
        sys.exit( 1 )

    return OGRDataSource

def addFeature(OGRDataSource,location,xlist,ylist):

    OGRLayer=OGRDataSource.GetLayer();
    OGRFeature = ogr.Feature(OGRLayer.GetLayerDefn())
    if OGRFeature is None:
        print('Could not create Feature')
        sys.exit( 1 )

    OGRFeature.SetField(TileIndexFieldName,location);
    wkt = 'POLYGON ((%f %f,%f %f,%f %f,%f %f,%f %f ))' % (xlist[0],ylist[0],
            xlist[1],ylist[1],xlist[2],ylist[2],xlist[3],ylist[3],xlist[0],ylist[0])
    OGRGeometry=ogr.CreateGeometryFromWkt(wkt,OGRLayer.GetSpatialRef())
    if (OGRGeometry is None):
        print('Could not create Geometry')
        sys.exit( 1 )

    OGRFeature.SetGeometryDirectly(OGRGeometry)

    OGRLayer.CreateFeature(OGRFeature)
    OGRFeature.Destroy()

def closeTileIndex(OGRDataSource):
    OGRDataSource.Destroy()


def buildPyramid(minfo,createdTileIndexDS,tileWidth, tileHeight):

    global LastRowIndx
    inputDS=createdTileIndexDS
    for level in range(1,Levels+1):
        LastRowIndx = -1
        levelMosaicInfo = mosaic_info(minfo.filename,inputDS)
        levelOutputTileInfo = tile_info(levelMosaicInfo.xsize/2,levelMosaicInfo.ysize/2,tileWidth,tileHeight)
        inputDS=buildPyramidLevel(levelMosaicInfo,levelOutputTileInfo,level)


def buildPyramidLevel(levelMosaicInfo,levelOutputTileInfo, level):
    yRange = list(range(1,levelOutputTileInfo.countTilesY+1))
    xRange = list(range(1,levelOutputTileInfo.countTilesX+1))

    OGRDS=createTileIndex("TileResult_"+str(level), TileIndexFieldName, Source_SRS,TileIndexDriverTyp)

    for yIndex in yRange:
        for xIndex in xRange:
            offsetY=(yIndex-1)* levelOutputTileInfo.tileHeight
            offsetX=(xIndex-1)* levelOutputTileInfo.tileWidth
            if yIndex==levelOutputTileInfo.countTilesY:
                height=levelOutputTileInfo.lastTileHeight
            else:
                height=levelOutputTileInfo.tileHeight

            if xIndex==levelOutputTileInfo.countTilesX:
                width=levelOutputTileInfo.lastTileWidth
            else:
                width=levelOutputTileInfo.tileWidth
            tilename=getTileName(levelMosaicInfo,levelOutputTileInfo, xIndex, yIndex,level)
            createPyramidTile(levelMosaicInfo, offsetX, offsetY, width, height,tilename,OGRDS)


    if TileIndexName is not None:
        shapeName=getTargetDir(level)+TileIndexName
        copyTileIndexToDisk(OGRDS,shapeName)

    if CsvFileName is not None:
        csvName=getTargetDir(level)+CsvFileName
        copyTileIndexToCSV(OGRDS,csvName)


    return OGRDS

def getTileName(minfo,ti,xIndex,yIndex,level = -1):
    """
    creates the tile file name
    """
    global LastRowIndx

    max = ti.countTilesX
    if (ti.countTilesY > max):
        max=ti.countTilesY
    countDigits= len(str(max))
    parts=os.path.splitext(os.path.basename(minfo.filename))
    if parts[0][0]=="@" : #remove possible leading "@"
       parts = ( parts[0][1:len(parts[0])], parts[1])

    if UseDirForEachRow :
        format=getTargetDir(level)+str(yIndex)+os.sep+parts[0]+"_%0"+str(countDigits)+"i"+"_%0"+str(countDigits)+"i"
        #See if there was a switch in the row, if so then create new dir for row.
        if LastRowIndx < yIndex :
            LastRowIndx = yIndex
            if (os.path.exists(getTargetDir(level)+str(yIndex)) == False) :
                os.mkdir(getTargetDir(level)+str(yIndex))
    else:
        format=getTargetDir(level)+parts[0]+"_%0"+str(countDigits)+"i"+"_%0"+str(countDigits)+"i"
    #Check for the extension that should be used.
    if Extension is None:
        format=format+parts[1]
    else:
        format=format+"."+Extension
    return format % (yIndex,xIndex)

def UsageFormat():
    print('Valid formats:')
    count = gdal.GetDriverCount()
    for index in range(count):
       driver= gdal.GetDriver(index)
       print(driver.ShortName)

# =============================================================================
def Usage():
     print('Usage: gdal_retile.py ')
     print('        [-v] [-co NAME=VALUE]* [-of out_format]')
     print('        [-ps pixelWidth pixelHeight]')
     print('        [-ot  {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/')
     print('               CInt16/CInt32/CFloat32/CFloat64}]')
     print('        [ -tileIndex tileIndexName [-tileIndexField fieldName]]')
     print('        [ -csv fileName [-csvDelim delimiter]]')
     print('        [-s_srs srs_def]  [-pyramidOnly] -levels numberoflevels')
     print('        [-r {near/bilinear/cubic/cubicspline/lanczos}]')
     print('        [-useDirForEachRow]')
     print('        -targetDir TileDirectory input_files')

# =============================================================================


# =============================================================================
#
# Program mainline.
#

def main(args = None):

    global Verbose
    global CreateOptions
    global Names
    global TileWidth
    global TileHeight
    global Format
    global BandType
    global Driver
    global Extension
    global MemDriver
    global TileIndexFieldName
    global TileIndexName
    global CsvDelimiter
    global CsvFileName

    global TileIndexDriverTyp
    global Source_SRS
    global TargetDir
    global ResamplingMethod
    global Levels
    global PyramidOnly
    global UseDirForEachRow

    gdal.AllRegister()

    if args is None:
        args = sys.argv
    argv = gdal.GeneralCmdLineProcessor( args )
    if argv is None:
        return 1

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-of':
            i+=1
            Format = argv[i]
        elif arg == '-ot':
            i+=1
            BandType = gdal.GetDataTypeByName( argv[i] )
            if BandType == gdal.GDT_Unknown:
                print('Unknown GDAL data type: %s' % argv[i])
                return 1
        elif arg == '-co':
            i+=1
            CreateOptions.append( argv[i] )


        elif arg == '-v':
            Verbose = True

        elif arg == '-targetDir':
            i+=1
            TargetDir=argv[i]

            if os.path.exists(TargetDir)==False:
                print("TargetDir " + TargetDir + " does not exist")
                return 1
            if TargetDir[len(TargetDir)-1:] != os.sep:
                TargetDir =  TargetDir+os.sep

        elif arg == '-ps':
            i+=1
            TileWidth=int(argv[i])
            i+=1
            TileHeight=int(argv[i])

        elif arg == '-r':
            i+=1
            ResamplingMethodString=argv[i]
            if ResamplingMethodString=="near":
                ResamplingMethod=GRA_NearestNeighbour
            elif ResamplingMethodString=="bilinear":
                 ResamplingMethod=GRA_Bilinear
            elif ResamplingMethodString=="cubic":
                 ResamplingMethod=GRA_Cubic
            elif ResamplingMethodString=="cubicspline":
                 ResamplingMethod=GRA_CubicSpline
            elif ResamplingMethodString=="lanczos":
                ResamplingMethod=GRA_Lanczos
            else:
                print("Unknown resampling method: %s" % ResamplingMethodString)
                return 1
        elif arg == '-levels':
            i+=1
            Levels=int(argv[i])
            if Levels<1:
                print("Invalid number of levels : %d" % Levels)
                return 1
        elif arg == '-s_srs':
            i+=1
            Source_SRS = osr.SpatialReference()
            if Source_SRS.SetFromUserInput( argv[i] ) != 0:
                print('invalid -s_srs: ' + argv[i]);
                return 1;

        elif arg ==  "-pyramidOnly":
            PyramidOnly=True
        elif arg == '-tileIndex':
            i+=1
            TileIndexName=argv[i]
            parts=os.path.splitext(TileIndexName)
            if len(parts[1])==0:
                TileIndexName+=".shp"

        elif arg == '-tileIndexField':
            i+=1
            TileIndexFieldName=argv[i]
        elif arg == '-csv':
            i+=1
            CsvFileName=argv[i]
            parts=os.path.splitext(CsvFileName)
            if len(parts[1])==0:
                CsvFileName+=".csv"
        elif arg == '-csvDelim':
            i+=1
            CsvDelimiter=argv[i]
        elif arg == '-useDirForEachRow':
            UseDirForEachRow=True
        elif arg[:1] == '-':
            print('Unrecognised command option: %s' % arg)
            Usage()
            return 1

        else:
            Names.append( arg )
        i+=1

    if len(Names) == 0:
        print('No input files selected.')
        Usage()
        return 1

    if (TileWidth==0 or TileHeight==0):
        print("Invalid tile dimension %d,%d" % (TileWidth,TileHeight))
        return 1

    if (TargetDir is None):
        print("Missing Directory for Tiles -targetDir")
        Usage()
        return 1

    # create level 0 directory if needed
    if(UseDirForEachRow and PyramidOnly==False) :
        leveldir=TargetDir+str(0)+os.sep
        if (os.path.exists(leveldir)==False):
            os.mkdir(leveldir)

    if Levels > 0:    #prepare Dirs for pyramid
        startIndx=1
        for levelIndx in range (startIndx,Levels+1):
            leveldir=TargetDir+str(levelIndx)+os.sep
            if (os.path.exists(leveldir)):
                continue
            os.mkdir(leveldir)
            if (os.path.exists(leveldir)==False):
                print("Cannot create level dir: %s" % leveldir)
                return 1
            if Verbose :
                print("Created level dir: %s" % leveldir)


    Driver = gdal.GetDriverByName(Format)
    if Driver is None:
        print('Format driver %s not found, pick a supported driver.' % Format)
        UsageFormat()
        return 1




    DriverMD = Driver.GetMetadata()
    Extension=DriverMD.get(DMD_EXTENSION);
    if 'DCAP_CREATE' not in DriverMD:
        MemDriver=gdal.GetDriverByName("MEM")


    tileIndexDS=getTileIndexFromFiles(Names,TileIndexDriverTyp)
    if tileIndexDS is None:
        print("Error building tile index")
        return 1;
    minfo = mosaic_info(Names[0],tileIndexDS)
    ti=tile_info(minfo.xsize,minfo.ysize, TileWidth, TileHeight)

    if Source_SRS is None and len(minfo.projection) > 0 :
       Source_SRS = osr.SpatialReference()
       if Source_SRS.SetFromUserInput( minfo.projection ) != 0:
           print('invalid projection  ' + minfo.projection);
           return 1

    if Verbose:
        minfo.report()
        ti.report()


    if PyramidOnly==False:
       dsCreatedTileIndex = tileImage(minfo,ti)
       tileIndexDS.Destroy()
    else:
       dsCreatedTileIndex=tileIndexDS

    if Levels>0:
       buildPyramid(minfo,dsCreatedTileIndex,TileWidth, TileHeight)

    if Verbose:
        print("FINISHED")
    return 0

def initGlobals():
    """ Only used for unit tests """
    global Verbose
    global CreateOptions
    global Names
    global TileWidth
    global TileHeight
    global Format
    global BandType
    global Driver
    global Extension
    global MemDriver
    global TileIndexFieldName
    global TileIndexName
    global TileIndexDriverTyp
    global CsvDelimiter
    global CsvFileName
    global Source_SRS
    global TargetDir
    global ResamplingMethod
    global Levels
    global PyramidOnly
    global LastRowIndx
    global UseDirForEachRow


    Verbose=False
    CreateOptions = []
    Names=[]
    TileWidth=256
    TileHeight=256
    Format='GTiff'
    BandType = None
    Driver=None
    Extension=None
    MemDriver=None
    TileIndexFieldName='location'
    TileIndexName=None
    TileIndexDriverTyp="Memory"
    CsvDelimiter=";"
    CsvFileName=None

    Source_SRS=None
    TargetDir=None
    ResamplingMethod=GRA_NearestNeighbour
    Levels=0
    PyramidOnly=False
    LastRowIndx=-1
    UseDirForEachRow=False



#global vars
Verbose=False
CreateOptions = []
Names=[]
TileWidth=256
TileHeight=256
Format='GTiff'
BandType = None
Driver=None
Extension=None
MemDriver=None
TileIndexFieldName='location'
TileIndexName=None
TileIndexDriverTyp="Memory"
CsvDelimiter=";"
CsvFileName=None
Source_SRS=None
TargetDir=None
ResamplingMethod=GRA_NearestNeighbour
Levels=0
PyramidOnly=False
LastRowIndx=-1
UseDirForEachRow=False


if __name__ == '__main__':
    sys.exit(main(sys.argv))
