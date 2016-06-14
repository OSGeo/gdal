#!/usr/bin/env ruby

require 'gdal/ogr'
require 'test/unit'
require 'ogrtest'

class TestOgrGeos < Test::Unit::TestCase
  def setup
    pnt1 = Gdal::Ogr::create_geometry_from_wkt( 'POINT(10 20)' )
    pnt2 = Gdal::Ogr::create_geometry_from_wkt( 'POINT(30 20)' )

    begin
      result = pnt1.union(pnt2)
    rescue
      result = nil
    end

    if result == nil
      @have_geos = false
    else
      @have_geos = true
    end
  end
  
  def test_union()
    return if not @have_geos
    
    pnt1 = Gdal::Ogr::create_geometry_from_wkt( 'POINT(10 20)' )
    pnt2 = Gdal::Ogr::create_geometry_from_wkt( 'POINT(30 20)' )
    result = pnt1.union(pnt2)

    assert(check_feature_geometry(result, 'MULTIPOINT (10 20,30 20)'))
  end

  def test_intersection()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 0 10, 10 0, 0 0))')
    
    result = g1.intersection(g2)
    assert(check_feature_geometry(result, 'POLYGON ((0 0,5 5,10 0,0 0))'))
  end

  def test_difference()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 0 10, 10 0, 0 0))')
    
    result = g1.difference(g2)
    assert(check_feature_geometry(result, 'POLYGON ((5 5,10 10,10 0,5 5))'))
  end


  def test_symmetric_difference()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 0 10, 10 0, 0 0))')
    
    result = g1.symmetric_difference(g2)
    assert(check_feature_geometry(result, 'MULTIPOLYGON (((5 5,0 0,0 10,5 5)),((5 5,10 10,10 0,5 5)))'))
  end

  def test_intersect()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(0 0, 10 10)')
    g2 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(10 0, 0 10)')
    
    result = g1.intersect(g2)
    assert(result)

    g1 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(0 0, 10 10)')
    g2 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((20 20, 20 30, 30 20, 20 20))')

    result = g1.intersect(g2)
    assert(!result)
  end
  
  def test_disjoint()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(0 0, 10 10)')
    g2 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(10 0, 0 10)')
    
    result = g1.disjoint(g2)
    assert(!result)

    g1 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(0 0, 10 10)')
    g2 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((20 20, 20 30, 30 20, 20 20))')

    result = g1.disjoint(g2)
    assert(result)
  end

  
  def test_touches()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(0 0, 10 10)')
    g2 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(0 0, 0 10)')
        
    result = g1.touches(g2)
    assert(result)

    g1 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(0 0, 10 10)')
    g2 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((20 20, 20 30, 30 20, 20 20))')

    result = g1.touches(g2)
    assert(!result)
  end

  
  def test_crosses()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(0 0, 10 10)')
    g2 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(10 0, 0 10)')
    
    result = g1.crosses(g2)
    assert(result)

    g1 = Gdal::Ogr::create_geometry_from_wkt('LINESTRING(0 0, 10 10)')
    g2 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((20 20, 20 30, 30 20, 20 20))')

    result = g1.crosses(g2)
    assert(!result)
  end

  def test_within()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((-90 -90, -90 90, 190 -90, -90 -90))')
    
    result = g1.within(g2)
    assert(result)

    result = g2.within(g1)
    assert(!result)
  end


  def test_contains()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((-90 -90, -90 90, 190 -90, -90 -90))')
    
    result = g2.contains(g1)
    assert(result)

    result = g1.contains(g2)
    assert(!result)
  end

  def test_overlaps()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    g2 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((-90 -90, -90 90, 190 -90, -90 -90))')
    
    result = g2.overlaps(g1)
    assert(!result)

    result = g1.overlaps(g2)
    assert(!result)
  end

  def test_centroid()
    return if not @have_geos
    
    g1 = Gdal::Ogr::create_geometry_from_wkt('POLYGON((0 0, 10 10, 10 0, 0 0))')
    
    result = g1.centroid()
    assert(check_feature_geometry(result, 'POINT(6.666666667 3.333333333)'))
  end
end