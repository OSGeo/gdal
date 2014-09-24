#!/usr/bin/env ruby

# FeatureDefn own FieldDefns.

require 'test/unit'
require 'gdal/ogr'
require 'ogrtest'

class TestMemoryFeatureDefn < Test::Unit::TestCase
  def open_ds
    # Get the pat to the file
    file_name = File.join(data_directory, 'poly.shp')
    
    # Open datasource in update mode
    return Gdal::Ogr.open(file_name, 1)
  end   

  def test_owns_field_defn
    ds = open_ds
    layer = ds.get_layer(0)
    feature_defn = layer.get_layer_defn
    field_defn = feature_defn.get_field_defn(0)

    feature_defn = nil
    layer = nil
    ds = nil

    # Now access the field defn
    assert_equal("AREA", field_defn.get_name, "Wrong field name")
  end

  def test_owns_field_defn
    ds = open_ds
    layer = ds.get_layer(0)

    feature = nil
    layer.each do |feat|
      feature = feat
    end

    field_defn = feature. get_field_defn_ref(0)

    layer = nil
    ds = nil

    # Now access the field defn
    assert_equal("AREA", field_defn.get_name, "Wrong field name")
  end
end
