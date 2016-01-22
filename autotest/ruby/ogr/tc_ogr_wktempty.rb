#!/usr/bin/env ruby

require 'gdal/ogr'
require 'test/unit'

class TestOgrWktEmpty < Test::Unit::TestCase

  def check_empty_geom1(geom_type)
		geom =Gdal:: Ogr.create_geometry_from_wkt("#{geom_type}(EMPTY)")
		wkt = geom.export_to_wkt

		return ("#{geom_type} EMPTY" == wkt)
	end
	
  def check_empty_geom2(geom_type)
		geom = Gdal::Ogr.create_geometry_from_wkt("#{geom_type} EMPTY")
		wkt = geom.export_to_wkt

		return ("#{geom_type} EMPTY" == wkt)
	end

	def test_ogr_wktempty_1()
		assert(check_empty_geom1('GEOMETRYCOLLECTION'))
	end
	
  def test_ogr_wktempty_2()
		assert(check_empty_geom1('MULTIPOLYGON'))
	end
	
	def test_ogr_wktempty_3()
		assert(check_empty_geom1('MULTILINESTRING'))
	end
	
	def test_ogr_wktempty_4()
		assert(check_empty_geom1('MULTIPOINT'))
	end

	def test_ogr_wktempty_5()
		geom = Gdal::Ogr.create_geometry_from_wkt("POINT(EMPTY)")
		wkt = geom.export_to_wkt
		assert_equal('POINT (0 0)', wkt)
	end

	def test_ogr_wktempty_6()
		assert(check_empty_geom1('LINESTRING'))
	end

	def test_ogr_wktempty_7()
		assert(check_empty_geom1('POLYGON'))
	end
	
	def test_ogr_wktempty_8()
		assert(check_empty_geom2('GEOMETRYCOLLECTION'))
	end

	def test_ogr_wktempty_9()
		assert(check_empty_geom2('MULTIPOLYGON'))
	end

	def test_ogr_wktempty_10()
		assert(check_empty_geom2('MULTILINESTRING'))
	end
	
	def test_ogr_wktempty_11()
		assert(check_empty_geom2('MULTIPOINT'))
	end
	
	def test_ogr_wktempty_12()
		geom = Gdal::Ogr.create_geometry_from_wkt("POINT EMPTY")
		wkt = geom.export_to_wkt
		assert_equal('POINT (0 0)', wkt)
	end
	
	def test_ogr_wktempty_13()
		assert(check_empty_geom2('LINESTRING'))
	end
	
	def test_ogr_wktempty_14()
		assert(check_empty_geom2('POLYGON'))
	end
end

