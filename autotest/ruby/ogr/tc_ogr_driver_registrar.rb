#!/usr/bin/env ruby

require 'gdal/ogr'
require 'test/unit'

class TestOgrDriverRegistrar < Test::Unit::TestCase
  def test_driver_count
    count = Gdal::Ogr.get_driver_count
    assert_not_nil(count);
	end 

  def test_driver_names
    count = Gdal::Ogr.get_driver_count

    0.upto(count-1) do |i|
      driver_by_index = Gdal::Ogr.get_driver(i)
      assert_not_nil(driver_by_index)
      
      driver_by_name = Gdal::Ogr.get_driver_by_name(driver_by_index.name)
      assert_not_nil(driver_by_name)

      assert_equal(driver_by_index.name, driver_by_name.name)
    end
  end
end

