#!/usr/bin/env ruby

# Datasources own layers and thus must not be freed while a
# layer is still referenced.  These tests verify this.

require 'test/unit'
require 'gdal/ogr'
require 'ogrtest'


class TestMemoryDataSource < Test::Unit::TestCase
  def open_ds
    # Get the pat to the file
    file_name = File.join(data_directory, 'poly.shp')

    # Open datasource in update mode
    return Gdal::Ogr.open(file_name, 1)
  end   

  def test_ds_owns_layer
    ds = open_ds
    layer = ds.get_layer(0)
    ds = nil

    assert_equal(10, layer.get_feature_count)
  end

  def test_ds_copy_layer
    ds = open_ds
    layer = ds.get_layer(0)
      
    # This will create a new layer in the data directory
    layer_copy = ds.copy_layer(layer, '../temp_copy.shp')
      
    assert_equal(10, layer_copy.get_feature_count)
  end

  def test_ds_create_layer
    ds = open_ds
    layer = ds.get_layer(0)
      
    # Create a new layer in the data directory
    new_layer = ds.create_layer('../temp_new.shp')
      
    assert_equal(0, new_layer.get_feature_count)
  end
end
