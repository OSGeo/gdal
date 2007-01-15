#!/usr/bin/env ruby

require 'test/unit'
require 'gdal/ogr'
require 'ogrtest'


class TestOgrBasic < Test::Unit::TestCase
  def setup
    file_name = File.join(data_directory, 'poly.shp')
    @ds = Gdal::Ogr.open(file_name)
  end   
  
  def teardown
    @ds = nil
  end 
  
  def test_ogr_basic_1
    assert_not_nil(@ds)  
  end

  def test_feature_counting
    expected_count = 10
  
    layer = @ds.get_layer('poly')
    layer.get_feature_count
    assert_equal(expected_count, layer.get_feature_count)
  
    # Now actually iterate through counting the features and ensure they agree.
    layer.reset_reading()

    count2 = 0
    feature = layer.get_next_feature()

    while not feature.nil?
      count2 += 1
      feature = layer.get_next_feature()
    end

    assert_equal(expected_count, count2)

    ## Now test using an a block
    layer.reset_reading()
    count3 = 0
    layer.each do |feature|
      count3 += 1
    end
    assert_equal(expected_count, count3)
  end

  def test_spatial_query
    minx = 479405
    miny = 4762826
    maxx = 480732
    maxy = 4763590

    # Create query geometry.
    ring = Gdal::Ogr::Geometry.new(Gdal::Ogr::WKBLINEARRING)
    ring.add_point( minx, miny )
    ring.add_point( maxx, miny )
    ring.add_point( maxx, maxy )
    ring.add_point( minx, maxy )
    ring.add_point( minx, miny )

    poly = Gdal::Ogr::Geometry.new(Gdal::Ogr::WKBPOLYGON)

    # Poly takes over owernship of ring here
    poly.add_geometry_directly( ring )

    layer = @ds.get_layer('poly')
    layer.set_spatial_filter(poly)
    layer.reset_reading()

    assert(1, layer.get_feature_count)

    feat1 = layer.get_next_feature
    feat2 = layer.get_next_feature

    assert_not_nil(feat1)
    assert_nil(feat2)
    
    layer.set_spatial_filter(nil)

    assert_equal(10, layer.get_feature_count)
  end
end
