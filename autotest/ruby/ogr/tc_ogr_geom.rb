#!/usr/bin/env ruby

require 'gdal/ogr'
require 'test/unit'

class TestOgrGeom < Test::Unit::TestCase
  
	# Test Area calculation for a MultiPolygon (which excersises lower level
  # get_Area() methods as well).

  def test_geom_area()
		geom_wkt = 'MULTIPOLYGON( ((0 0,1 1,1 0,0 0)),((0 0,10 0, 10 10, 0 10),(1 1,1 2,2 2,2 1)) )'
    geom = Gdal::Ogr.create_geometry_from_wkt( geom_wkt )

    area = geom.get_area()
    assert_in_delta(99.5, area, 0.00000000001)
	end
end