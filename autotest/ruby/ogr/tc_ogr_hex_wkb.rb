#!/usr/bin/env ruby

require 'gdal/gdal'
require 'gdal/ogr'
require 'test/unit'

class TestOgrHexWkb < Test::Unit::TestCase
  def compare(geom1)
    # output to wkb
    wkb1 = geom1.export_to_wkb(Gdal::Ogr::WKBNDR)
    # convert to hex
    hex1 = Gdal::Gdal::binary_to_hex(wkb1)
    # convert back to binary
    wkb2 = Gdal::Gdal::hex_to_binary(hex1)
    # get the geom
    geom2 = Gdal::Ogr::create_geometry_from_wkb(wkb2)
    # assert these are the same
    assert(geom2.equal(geom1))
  end
  
  def test_point   
    geom = Gdal::Ogr::create_geometry_from_wkt( 'POINT(10 20)' )
    self.compare(geom)
  end
  
  def test_linestring 
    geom = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(0 0, 10 10)')
    self.compare(geom)
  end
  
  def test_polygon 
    geom = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    self.compare(geom)
  end
end