#!/usr/bin/env ruby

require 'gdal/ogr'
require 'ogrtest'
require 'test/unit'

class TestOgrShape < Test::Unit::TestCase
  def test_seg1
    temp_dir = "c:/temp/csvwrk"
		driver =  Gdal::Ogr.get_driver_by_name('CSV')
		driver.delete_data_source(temp_dir)

		ds = driver.create_data_source(temp_dir)

		layer = ds.create_layer('pm1')
		quick_create_layer_def(layer,
													 [ ['PRIME_MERIDIAN_CODE', Gdal::Ogr::OFTInteger],
														 ['INFORMATION_SOURCE',Gdal:: Ogr::OFTString] ] )

		dst_feat = Gdal::Ogr::Feature.new(feature_def = layer.get_layer_defn())
		ds.delete_layer(0)
  end
end
