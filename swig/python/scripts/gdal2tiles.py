#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  Google Summer of Code 2007
# Purpose:  Convert a raster into TMS tiles, create KML SuperOverlay EPSG:4326,
#           generate a simple HTML viewers based on Google Maps and OpenLayers
# Author:   Klokan Petr Pridal, klokan at klokan dot cz
# Web:      http://www.klokan.cz/projects/gdal2tiles/
#
###############################################################################
# Copyright (c) 2007, Klokan Petr Pridal
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################
# 
#  $Log$
#

import gdal
import sys, os, tempfile
from gdalconst import GA_ReadOnly
from osr import SpatialReference
from math import ceil, log10
import operator

verbose = False

tilesize = 256
tileformat = 'png'

tempdriver = gdal.GetDriverByName( 'MEM' )
tiledriver = gdal.GetDriverByName( tileformat )

# =============================================================================
def writetile( filename, data, dxsize, dysize, bands):
    """
    Write raster 'data' (of the size 'dataxsize' x 'dataysize') read from
    'dataset' into the tile 'filename' with size 'tilesize' pixels.
    Later this should be replaced by new <TMS Tile Raster Driver> from GDAL.
    """

    # Create needed directories for output
    dirs, file = os.path.split(filename)
    if not os.path.isdir(dirs):
        os.makedirs(dirs)

    # GRR, PNG DRIVER DOESN'T HAVE CREATE() !!!
    # so we have to create temporary file in memmory...

    #TODO: Add transparency to files with one band only too (grayscale).
    if bands == 3 and tileformat == 'png':
        tmp = tempdriver.Create('', tilesize, tilesize, bands=4)
        alpha = tmp.GetRasterBand(4)
        #from Numeric import zeros
        alphaarray = (zeros((dysize, dxsize)) + 255).astype('b')
        alpha.WriteArray( alphaarray, 0, tilesize-dysize )
    else:
        tmp = tempdriver.Create('', tilesize, tilesize, bands=bands)

    # (write data from the bottom left corner)
    tmp.WriteRaster( 0, tilesize-dysize, dxsize, dysize, data, band_list=range(1, bands+1))
 
    # ... and then copy it into the final tile with given filename
    tiledriver.CreateCopy(filename, tmp, strict=0)

    return 0


# =============================================================================
def generate_tilemapresource( **args ):
    """
    Template for tilemapresource.xml. Returns filled string. Expected variables:
      title, north, south, east, west, isepsg4326, projection, publishurl,
      zoompixels, tilesize, tileformat, profile
    """

    if args['isepsg4326']:
        args['srs'] = "EPSG:4326"
    else:
        args['srs'] = args['projection']

    zoompixels = args['zoompixels']

    s = """<?xml version="1.0" encoding="utf-8"?>
<TileMap version="1.0.0" tilemapservice="http://tms.osgeo.org/1.0.0">
  <Title>%(title)s</Title>
  <Abstract></Abstract>
  <SRS>%(srs)s</SRS>
  <BoundingBox minx="%(south).20f" miny="%(west).20f" maxx="%(north).20f" maxy="%(east).20f"/>
  <Origin x="%(south).20f" y="%(west).20f"/>
  <TileFormat width="%(tilesize)d" height="%(tilesize)d" mime-type="image/%(tileformat)s" extension="%(tileformat)s"/>
  <TileSets profile="%(profile)s">
""" % args
    for z in range(len(zoompixels)):
        s += """    <TileSet href="%s%d" units-per-pixel="%.20f" order="%d"/>\n""" % (args['publishurl'], z, zoompixels[z], z)
    s += """  </TileSets>
</TileMap>
"""
    return s

# =============================================================================
def generate_rootkml( **args ):
    """
    Template for the root doc.kml. Returns filled string. Expected variables:
      title, north, south, east, west, tilesize, tileformat, publishurl
    """
    
    args['minlodpixels'] = args['tilesize'] / 2

    s = """<?xml version="1.0" encoding="utf-8"?>
<kml xmlns="http://earth.google.com/kml/2.1">
  <Document>
    <name>%(title)s</name>
    <description></description>
    <Style>
      <ListStyle id="hideChildren">
        <listItemType>checkHideChildren</listItemType>
      </ListStyle>
    </Style>
    <Region>
      <LatLonAltBox>
        <north>%(north).20f</north>
        <south>%(south).20f</south>
        <east>%(east).20f</east>
        <west>%(west).20f</west>
      </LatLonAltBox>
    </Region>
    <NetworkLink>
      <open>1</open>
      <Region>
        <Lod>
          <minLodPixels>%(minlodpixels)d</minLodPixels>
          <maxLodPixels>-1</maxLodPixels>
        </Lod>
        <LatLonAltBox>
          <north>%(north).20f</north>
          <south>%(south).20f</south>
          <east>%(east).20f</east>
          <west>%(west).20f</west>
        </LatLonAltBox>
      </Region>
      <Link>
        <href>%(publishurl)s0/0/0.kml</href>
        <viewRefreshMode>onRegion</viewRefreshMode>
      </Link>
    </NetworkLink>
  </Document>
</kml>
""" % args
    return s

# =============================================================================
def generate_kml( **args ):
    """
    Template for the tile kml. Returns filled string. Expected variables:
      zoom, ix, iy, rpixel, tilesize, tileformat, south, west, xsize, ysize,
      maxzoom
    """

    zoom, ix, iy, rpixel = args['zoom'], args['ix'], args['iy'], args['rpixel']
    maxzoom, tilesize = args['maxzoom'], args['tilesize']
    south, west = args['south'], args['west']
    xsize, ysize = args['xsize'], args['ysize']

    nsew = lambda ix, iy, rpixel: (south + rpixel*((iy+1)*tilesize),
                                    south + rpixel*(iy*tilesize),
                                    west + rpixel*((ix+1)*tilesize),
                                    west + rpixel*ix*tilesize)

    args['minlodpixels'] = args['tilesize'] / 2
    args['tnorth'], args['tsouth'], args['teast'], args['twest'] = nsew(ix, iy, rpixel)

    if verbose:
        print "\tKML for area NSEW: %.20f %.20f %.20f %.20f" % nsew(ix, iy, rpixel)

    xchildern = []
    ychildern = []
    if zoom < maxzoom:
        zareasize = 2.0**(maxzoom-zoom-1)*tilesize
        xchildern.append(ix*2)
        if ix*2+1 < int( ceil( xsize / zareasize)):
            xchildern.append(ix*2+1)
        ychildern.append(iy*2)
        if iy*2+1 < int( ceil( ysize / zareasize)):
            ychildern.append(iy*2+1)

    s = """<?xml version="1.0" encoding="utf-8"?>
<kml xmlns="http://earth.google.com/kml/2.1">
  <Document>
    <name>%(zoom)d/%(ix)d/%(iy)d.kml</name>
    <Region>
      <Lod>
        <minLodPixels>%(minlodpixels)d</minLodPixels>
        <maxLodPixels>-1</maxLodPixels>
      </Lod>
      <LatLonAltBox>
        <north>%(tnorth).20f</north>
        <south>%(tsouth).20f</south>
        <east>%(teast).20f</east>
        <west>%(twest).20f</west>
      </LatLonAltBox>
    </Region>
    <GroundOverlay>
      <drawOrder>%(zoom)d</drawOrder>
      <Icon>
        <href>%(iy)d.%(tileformat)s</href>
      </Icon>
      <LatLonBox>
        <north>%(tnorth).20f</north>
        <south>%(tsouth).20f</south>
        <east>%(teast).20f</east>
        <west>%(twest).20f</west>
      </LatLonBox>
    </GroundOverlay>
""" % args

    for cx in xchildern:
        for cy in ychildern:

            if verbose:
                print "\t  ", [cx, cy], "NSEW: %.20f %.20f %.20f %.20f" % nsew(cx, cy, rpixel/2)

            cnorth, csouth, ceast, cwest = nsew(cx, cy, rpixel/2)
            s += """    <NetworkLink>
      <name>%d/%d/%d.png</name>
      <Region>
        <Lod>
          <minLodPixels>%d</minLodPixels>
          <maxLodPixels>-1</maxLodPixels>
        </Lod>
        <LatLonAltBox>
          <north>%.20f</north>
          <south>%.20f</south>
          <east>%.20f</east>
          <west>%.20f</west>
        </LatLonAltBox>
      </Region>
      <Link>
        <href>../../%d/%d/%d.kml</href>
        <viewRefreshMode>onRegion</viewRefreshMode>
        <viewFormat/>
      </Link>
    </NetworkLink>
""" % (zoom+1, cx, cy, args['minlodpixels'], cnorth, csouth, ceast, cwest, zoom+1, cx, cy)

    s += """  </Document>
</kml>
"""
    return s

# =============================================================================
def generate_googlemaps( **args ):
    """
    Template for googlemaps.html. Returns filled string. Expected variables:
      title, googlemapskey, xsize, ysize, maxzoom, tilesize, 
    """

    args['zoom'] = min( 3, args['maxzoom'])

    s = """<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:v="urn:schemas-microsoft-com:vml"> 
  <head>
    <title>%(title)s</title>
    <meta http-equiv="content-type" content="text/html; charset=utf-8"/>
    <meta http-equiv='imagetoolbar' content='no'/>
    <style type="text/css"> v\:* {behavior:url(#default#VML);}
        html, body { overflow: hidden; padding: 0; height: 100%%; width: 100%%; font-family: 'Lucida Grande',Geneva,Arial,Verdana,sans-serif; }
        body { margin: 10px; background: #fff; }
        h1 { margin: 0; padding: 6px; border:0; font-size: 20pt; }
        #header { height: 43px; padding: 0; background-color: #eee; border: 1px solid #888; }
        #subheader { height: 12px; text-align: right; font-size: 10px; color: #555;}
        #map { height: 95%%; border: 1px solid #888; }
    </style>
    <script src='http://maps.google.com/maps?file=api&amp;v=2.x&amp;key=%(googlemapskey)s' type='text/javascript'></script>
    <script type="text/javascript">
    //<![CDATA[

    function getWindowHeight() {
	    if (self.innerHeight) return self.innerHeight;
	    if (document.documentElement && document.documentElement.clientHeight)
	        return document.documentElement.clientHeight;
	    if (document.body) return document.body.clientHeight;
	    return 0;
    }

    function getWindowWidth() {
	    if (self.innerWidth) return self.innerWidth;
    	if (document.documentElement && document.documentElement.clientWidth)
	        return document.documentElement.clientWidth;
	    if (document.body) return document.body.clientWidth;
	    return 0;
    }
    
    function resize() {  
	    var map = document.getElementById("map");  
	    var header = document.getElementById("header");  
	    var subheader = document.getElementById("subheader");  
	    map.style.height = (getWindowHeight()-80) + "px";
	    map.style.width = (getWindowWidth()-20) + "px";
	    header.style.width = (getWindowWidth()-20) + "px";
	    subheader.style.width = (getWindowWidth()-20) + "px";
	    // map.checkResize();
    } 


	// See http://www.google.com/apis/maps/documentation/reference.html#GProjection 
	// This code comes from FlatProjection.js, done by Smurf in project gwmap
	/**
	 * Creates a custom GProjection for flat maps.
	 *
	 * @classDescription	Creates a custom GProjection for flat maps.
	 * @param {Number} width The width in pixels of the map at the specified zoom level.
	 * @param {Number} height The height in pixels of the map at the specified zoom level.
	 * @param {Number} pixelsPerLon The number of pixels per degree of longitude at the specified zoom level.
	 * @param {Number} zoom The zoom level width, height, and pixelsPerLon are set for.
	 * @param {Number} maxZoom The maximum zoom level the map will go.
	 * @constructor	
	 */
	function FlatProjection(width,height,pixelsPerLon,zoom,maxZoom)
	{
		this.pixelsPerLonDegree = new Array(maxZoom);
		this.tileBounds = new Array(maxZoom);

		width /= Math.pow(2,zoom);
		height /= Math.pow(2,zoom);
		pixelsPerLon /= Math.pow(2,zoom);
		
		for(var i=maxZoom; i>=0; i--)
		{
			this.pixelsPerLonDegree[i] = pixelsPerLon*Math.pow(2,i);
			this.tileBounds[i] = new GPoint(Math.ceil(width*Math.pow(2,i)/256), Math.ceil(height*Math.pow(2,i)/256));
		}
	}

	FlatProjection.prototype = new GProjection();

	FlatProjection.prototype.fromLatLngToPixel = function(point,zoom)
	{
		var x = Math.round(point.lng() * this.pixelsPerLonDegree[zoom]);
		var y = Math.round(point.lat() * this.pixelsPerLonDegree[zoom]);
		return new GPoint(x,y);
	}

	FlatProjection.prototype.fromPixelToLatLng = function(pixel,zoom,unbounded)	
	{
		var lng = pixel.x/this.pixelsPerLonDegree[zoom];
		var lat = pixel.y/this.pixelsPerLonDegree[zoom];
		return new GLatLng(lat,lng,true);
	}

	FlatProjection.prototype.tileCheckRange = function(tile, zoom, tilesize)
	{
		if( tile.y<0 || tile.x<0 || tile.y>=this.tileBounds[zoom].y || tile.x>=this.tileBounds[zoom].x )
		{
			return false;
		}
		return true;
	}
	FlatProjection.prototype.getWrapWidth = function(zoom)
	{
		return Number.MAX_VALUE;
	}

    /*
     * Main load function:
     */

    function load() {
        var MapWidth = %(xsize)d;
        var MapHeight = %(ysize)d;
        var MapMaxZoom = %(maxzoom)d;
        var MapPxPerLon = %(tilesize)d;

        if (GBrowserIsCompatible()) {
            var map = new GMap2( document.getElementById("map") );
            var tileLayer = [ new GTileLayer( new GCopyrightCollection(null), 0, MapMaxZoom ) ];
            tileLayer[0].getTileUrl = function(a,b) {
                var y = Math.floor(MapHeight / (MapPxPerLon * Math.pow(2, (MapMaxZoom-b)))) - a.y;
                // Google Maps indexed tiles from top left, we from bottom left, it causes problems during zooming, solution?
                return b+"/"+a.x+"/"+y+".png";
            }
            var mapType = new GMapType(
                tileLayer,
                new FlatProjection( MapWidth, MapHeight, MapPxPerLon, MapMaxZoom-2, MapMaxZoom),
                'Default',
                { maxResolution: MapMaxZoom, minResolution: 0, tileSize: MapPxPerLon }
            );
            map.addMapType(mapType);

            map.removeMapType(G_NORMAL_MAP);
            map.removeMapType(G_SATELLITE_MAP);
            map.removeMapType(G_HYBRID_MAP);

            map.setCenter(new GLatLng((MapHeight/MapPxPerLon/4)/2, (MapWidth/MapPxPerLon/4)/2), %(zoom)d, mapType);

            //alert((MapHeight/MapPxPerLon/4)/2 + " x " + (MapWidth/MapPxPerLon/4)/2);
            //map.addOverlay(new GMarker( new GLatLng((MapHeight/MapPxPerLon/4)/2,(MapWidth/MapPxPerLon/4)/2) ));

            map.getContainer().style.backgroundColor='#fff';

            map.addControl(new GLargeMapControl());
            // Overview Map Control is not running correctly...
            // map.addControl(new GOverviewMapControl());
        }
        resize();
    }

    onresize=function(){ resize(); };

    //]]>
    </script>
  </head>
  <body onload="load()" onunload="GUnload()">
      <div id="header"><h1>%(title)s</h1></div>
      <div id="subheader">Generated by <a href="http://www.klokan.cz/projects/gdal2tiles/">GDAL2Tiles</a>, Copyright (c) 2007 <a href="http://www.klokan.cz/">Klokan Petr Pridal</a>,  <a href="http://www.gdal.org/">GDAL</a> &amp; <a href="http://www.osgeo.org/">OSGeo</a> <a href="http://code.google.com/soc/">SoC 2007</a>
      </div>
       <div id="map"></div>
  </body>
</html>
""" % args

    return s

# =============================================================================
def generate_openlayers( **args ):
    """
    Template for openlayers.html. Returns filled string. Expected variables:
        title, xsize, ysize, maxzoom, tileformat
    """

    args['maxresolution'] = 2**(args['maxzoom'])
    args['zoom'] = min( 3, args['maxzoom'])
    args['maxzoom'] += 1

    s = """<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:v="urn:schemas-microsoft-com:vml"> 
  <head>
    <title>%(title)s</title>
    <meta http-equiv="content-type" content="text/html; charset=utf-8"/>
    <meta http-equiv='imagetoolbar' content='no'/>
    <style type="text/css"> v\:* {behavior:url(#default#VML);}
        html, body { overflow: hidden; padding: 0; height: 100%%; width: 100%%; font-family: 'Lucida Grande',Geneva,Arial,Verdana,sans-serif; }
        body { margin: 10px; background: #fff; }
        h1 { margin: 0; padding: 6px; border:0; font-size: 20pt; }
        #header { height: 43px; padding: 0; background-color: #eee; border: 1px solid #888; }
        #subheader { height: 12px; text-align: right; font-size: 10px; color: #555;}
        #map { height: 100%%; border: 1px solid #888; background-color: #fff; }
    </style>
    <script src="http://www.openlayers.org/api/OpenLayers.js" type="text/javascript"></script>
    <script type="text/javascript">
    //<![CDATA[
    var map, layer;

    function load(){
        // I realized sometimes OpenLayers has some troubles with Opera and Safari.. :-( Advices or patch are welcome...
        // Correct TMS Driver should read TileMapResource by OpenLayers.loadURL and parseXMLStringOpenLayers.parseXMLString
        // For correct projection the patch for OpenLayers TMS is needed, index to tiles (both x an y) has to be rounded into integers
        /* Then definition is like this:
        var options = {
            controls: [],
            // maxExtent: new OpenLayers.Bounds(13.7169981668, 49.608691789, 13.9325582389, 49.7489724456),
            maxExtent: new OpenLayers.Bounds(13.71699816677651995178, 49.46841113248020604942, 13.93255823887490052471, 49.60869178902321863234),
            maxResolution: 0.00150183372679037197,
            numZoomLevels: 6,
            units: 'degrees',
            projection: "EPSG:4326"
        };
        map = new OpenLayers.Map("map", options);
        */

        /* Just pixel based view now */
        var options = {
            controls: [],
            maxExtent: new OpenLayers.Bounds(0, 0, %(xsize)d, %(ysize)d),
            maxResolution: %(maxresolution)d,
            numZoomLevels: %(maxzoom)d,
            units: 'pixels',
            projection: ""
        };
        map = new OpenLayers.Map("map", options);

        // map.addControl(new OpenLayers.Control.MousePosition());
        map.addControl(new OpenLayers.Control.PanZoomBar());
        map.addControl(new OpenLayers.Control.MouseDefaults());
        map.addControl(new OpenLayers.Control.KeyboardDefaults());

        // Patch for OpenLayers TMS is needed because string "1.0.0" is hardcoded in url no,
        // there has to be optional parameter with version (default this "1.0.0")
        // Hack to support local tiles by stable OpenLayers branch without a patch
        OpenLayers.Layer.TMS.prototype.getURL = function ( bounds ) {
            bounds = this.adjustBoundsByGutter(bounds);
            var res = this.map.getResolution();
            var x = Math.round((bounds.left - this.tileOrigin.lon) / (res * this.tileSize.w));
            var y = Math.round((bounds.bottom - this.tileOrigin.lat) / (res * this.tileSize.h));
            var z = this.map.getZoom();
            var path = z + "/" + x + "/" + y + "." + this.type;
            var url = this.url;
            if (url instanceof Array) {
                url = this.selectUrl(path, url);
            }
            return url + path;
        };
        layer = new OpenLayers.Layer.TMS( "TMS", 
                "", {layername: 'map', type:'%(tileformat)s'} );
        map.addLayer(layer);
        map.zoomTo(%(zoom)d);

        resize();
    }

    function getWindowHeight() {
        if (self.innerHeight) return self.innerHeight;
        if (document.documentElement && document.documentElement.clientHeight)
            return document.documentElement.clientHeight;
        if (document.body) return document.body.clientHeight;
	        return 0;
    }

    function getWindowWidth() {
	    if (self.innerWidth) return self.innerWidth;
	    if (document.documentElement && document.documentElement.clientWidth)
	        return document.documentElement.clientWidth;
	    if (document.body) return document.body.clientWidth;
	        return 0;
    }
    
    function resize() {  
	    var map = document.getElementById("map");  
	    var header = document.getElementById("header");  
	    var subheader = document.getElementById("subheader");  
	    map.style.height = (getWindowHeight()-80) + "px";
	    map.style.width = (getWindowWidth()-20) + "px";
	    header.style.width = (getWindowWidth()-20) + "px";
	    subheader.style.width = (getWindowWidth()-20) + "px";
    } 

    onresize=function(){ resize(); };

    //]]>
    </script>
  </head>
  <body onload="load()">
      <div id="header"><h1>%(title)s</h1></div>
      <div id="subheader">Generated by <a href="http://www.klokan.cz/projects/gdal2tiles/">GDAL2Tiles</a>, Copyright (C) 2007 <a href="http://www.klokan.cz/">Klokan Petr Pridal</a>,  <a href="http://www.gdal.org/">GDAL</a> &amp; <a href="http://www.osgeo.org/">OSGeo</a> <a href="http://code.google.com/soc/">SoC 2007</a>
      </div>
       <div id="map"></div>
  </body>
</html>
""" % args

    return s

# =============================================================================
def Usage():
    print 'Usage: gdal2tiles.py [-title "Title"] [-publishurl http://yourserver/dir/]'
    print '                     [-nogooglemaps] [-noopenlayers] [-nokml]'
    print '                     [-googlemapskey KEY] [-forcekml] [-v]'
    print '                     input_file [output_dir]'
    print

# =============================================================================
#
# Program mainline.
#
# =============================================================================

if __name__ == '__main__':

    profile = 'local' # later there should be support for TMS global profiles
    title = ''
    publishurl = ''
    googlemapskey = 'INSERT_YOUR_KEY_HERE' # when not supplied as parameter
    nogooglemaps = False
    noopenlayers = False
    nokml = False
    forcekml = False

    input_file = ''
    output_dir = ''

    isepsg4326 = False
    generatekml = False

    gdal.AllRegister()
    argv = gdal.GeneralCmdLineProcessor( sys.argv )
    if argv is None:
        sys.exit( 0 )

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-v':
            verbose = True

        elif arg == '-nogooglemaps':
            nogooglemaps = True

        elif arg == '-noopenlayers':
            noopenlayers = True

        elif arg == '-nokml':
            nokml = True

        elif arg == '-forcekml':
            forcekml = True

        elif arg == '-title':
            i += 1
            title = argv[i]

        elif arg == '-publishurl':
            i += 1
            publishurl = argv[i]

        elif arg == '-googlemapskey':
            i += 1
            googlemapskey = argv[i]

        elif arg[:1] == '-':
            print >>sys.stderr, 'Unrecognised command option: ', arg
            Usage()
            sys.exit( 1 )

        elif not input_file:
            input_file = argv[i]

        elif not output_dir:
            output_dir = argv[i]

        else:
            print >>sys.stderr, 'Too many parameters already: ', arg
            Usage()
            sys.exit( 1 )
            
        i = i + 1

    if not input_file:
        print >>sys.stderr, 'No input_file was given.'
        Usage()
        sys.exit( 1 )

    # Set correct default values.
    if not title:
        title = os.path.basename( input_file )
    if not output_dir:
        output_dir = os.path.splitext(os.path.basename( input_file ))[0]
    if publishurl and not publishurl.endswith('/'):
        publishurl += '/'
    if publishurl:
        publishurl += os.path.basename(output_dir) + '/'

    # Open input_file and get all necessary information.
    dataset = gdal.Open( input_file, GA_ReadOnly )
    if dataset is None:
        Usage()
        sys.exit( 1 )
        
    bands = dataset.RasterCount
    if bands == 3 and tileformat == 'png':
        from Numeric import zeros
    xsize = dataset.RasterXSize
    ysize = dataset.RasterYSize

    geotransform = dataset.GetGeoTransform()
    projection = dataset.GetProjection()

    north = geotransform[3]
    south = geotransform[3]+geotransform[5]*ysize
    east = geotransform[0]+geotransform[1]*xsize
    west = geotransform[0]

    if verbose:
        print "Input (%s):" % input_file
        print "="*80
        print "  Driver:", dataset.GetDriver().ShortName,'/', dataset.GetDriver().LongName
        print "  Size:", xsize, 'x', ysize, 'x', bands
        print "  Projection:", projection
        print "  NSEW: ", (north, south, east, west) 

    if projection:
        # CHECK: Is there better way how to test that given file is in EPSG:4326?
        #spatialreference = SpatialReference(wkt=projection)
        #if spatialreference.???() == 4326:
        if projection.endswith('AUTHORITY["EPSG","4326"]]'):
            isepsg4326 = True
            if verbose:
                print "Projection detected as EPSG:4326"

    if (isepsg4326 or forcekml) and (north, south, east, west) != (0, xsize, ysize, 0):
        generatekml = True
    if verbose:
        if generatekml:
            print "Generating of KML is possible"
        else:
            print "It is not possible to generate KML (projection is not EPSG:4326 or there are no coordinates)!"

    if forcekml and (north, south, east, west) == (0, xsize, ysize, 0):
        print >> sys.stderr, "Geographic coordinates not available for given file '%s'" % input_file
        print >> sys.stderr, "so KML file can not be generated."
        sys.exit( 1 )


    # Python 2.2 compatibility.
    log2 = lambda x: log10(x) / log10(2) # log2 (base 2 logarithm)
    sum = lambda seq, start=0: reduce( operator.add, seq, start)

    # Zoom levels of the pyramid.
    maxzoom = int(max( ceil(log2(xsize/float(tilesize))), ceil(log2(ysize/float(tilesize)))))
    zoompixels = [geotransform[1] * 2.0**(maxzoom-zoom) for zoom in range(0, maxzoom+1)]
    tilecount = sum( [
        int( ceil( xsize / (2.0**(maxzoom-zoom)*tilesize))) * \
        int( ceil( ysize / (2.0**(maxzoom-zoom)*tilesize))) \
        for zoom in range(maxzoom+1)
    ] )

    # Create output directory, if it doesn't exist
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    if verbose:
        print "Output (%s):" % output_dir
        print "="*80
        print "  Format of tiles:", tiledriver.ShortName, '/', tiledriver.LongName
        print "  Size of a tile:", tilesize, 'x', tilesize, 'pixels'
        print "  Count of tiles:", tilecount
        print "  Zoom levels of the pyramid:", maxzoom
        print "  Pixel resolution by zoomlevels:", zoompixels

    # Generate tilemapresource.xml.
    f = open(os.path.join(output_dir, 'tilemapresource.xml'), 'w')
    f.write( generate_tilemapresource( 
        title = title,
        north = north,
        south = south,
        east = east,
        west = west,
        isepsg4326 = isepsg4326,
        projection = projection,
        publishurl = publishurl,
        zoompixels = zoompixels,
        tilesize = tilesize,
        tileformat = tileformat,
        profile = profile
    ))
    f.close()

    # Generate googlemaps.html
    if not nogooglemaps:
        f = open(os.path.join(output_dir, 'googlemaps.html'), 'w')
        f.write( generate_googlemaps(
          title = title,
          googlemapskey = googlemapskey,
          xsize = xsize,
          ysize = ysize,
          maxzoom = maxzoom,
          tilesize = tilesize
        ))
        f.close()

    # Generate openlayers.html
    if not noopenlayers:
        f = open(os.path.join(output_dir, 'openlayers.html'), 'w')
        f.write( generate_openlayers(
          title = title,
          xsize = xsize,
          ysize = ysize,
          maxzoom = maxzoom,
          tileformat = tileformat
        ))
        f.close()
        
    # Generate Root KML
    if generatekml:
        f = open(os.path.join(output_dir, 'doc.kml'), 'w')
        f.write( generate_rootkml(
            title = title,
            north = north,
            south = south,
            east = east,
            west = west,
            tilesize = tilesize,
            tileformat = tileformat,
            publishurl = ""
        ))
        f.close()
        
    # Generate Root KML with publishurl
    if generatekml and publishurl:
        f = open(os.path.join(output_dir, os.path.basename(output_dir)+'.kml'), 'w')
        f.write( generate_rootkml(
            title = title,
            north = north,
            south = south,
            east = east,
            west = west,
            tilesize = tilesize,
            tileformat = tileformat,
            publishurl = publishurl
        ))
        f.close()

    #
    # Main cycle for TILE and KML generating.
    #
    tileno = 0
    progress = 0
    for zoom in range(maxzoom, -1, -1):

        # Maximal size of read window in pixels.
        rmaxsize = 2.0**(maxzoom-zoom)*tilesize

        if verbose:
            print "-"*80
            print "Zoom %s - pixel %.20f" % (zoom, zoompixels[zoom]), int(2.0**zoom*tilesize)
            print "-"*80

        for ix in range(0, int( ceil( xsize / rmaxsize))):

            # Read window xsize in pixels.
            if ix+1 == int( ceil( xsize / rmaxsize)) and xsize % rmaxsize != 0:
                rxsize = int(xsize % rmaxsize)
            else:
                rxsize = int(rmaxsize)
            
            # Read window left coordinate in pixels.
            rx = int(ix * rmaxsize)

            for iy in range(0, int( ceil( ysize / rmaxsize))):

                # Read window ysize in pixels.
                if iy+1 == int( ceil( ysize / rmaxsize)) and ysize % rmaxsize != 0:
                    rysize = int(ysize % rmaxsize)
                else:
                    rysize = int(rmaxsize)

                # Read window top coordinate in pixels.
                ry = int(ysize - (iy * rmaxsize)) - rysize

                dxsize = int(rxsize/rmaxsize * tilesize)
                dysize = int(rysize/rmaxsize * tilesize)
                filename = os.path.join(output_dir, '%d/%d/%d.png' % (zoom, ix, iy))

                if verbose:
                    # Print info about tile and read area.
                    print "%d/%d" % (tileno+1,tilecount), filename, [ix, iy], [rx, ry], [rxsize, rysize], [dxsize, dysize]
                else:
                    # Show the progress bar.
                    percent = int(ceil((tileno) / float(tilecount-1) * 100))
                    while progress <= percent:
                        if progress % 10 == 0:
                            sys.stdout.write( "%d" % progress )
                            sys.stdout.flush()
                        else:
                            sys.stdout.write( '.' )
                            sys.stdout.flush()
                        progress += 2.5
               
                # Load raster from read window.
                data = dataset.ReadRaster(rx, ry, rxsize, rysize, dxsize, dysize)
                # Write that raster to the tile.
                writetile( filename, data, dxsize, dysize, bands)
               
                # Create a KML file for this tile.
                if generatekml:
                    f = open( os.path.join(output_dir, '%d/%d/%d.kml' % (zoom, ix, iy)), 'w')
                    f.write( generate_kml(
                        zoom = zoom,
                        ix = ix,
                        iy = iy,
                        rpixel = zoompixels[zoom],
                        tilesize = tilesize,
                        tileformat = tileformat,
                        south = south,
                        west = west,
                        xsize = xsize,
                        ysize = ysize,
                        maxzoom = maxzoom
                    ))
                    f.close()

                tileno += 1

    # Last \n for the progress bar
    print "\nDone"
