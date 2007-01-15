#!/usr/bin/env ruby

# Layers reference feature_defn as do features.  Feature_defns
# are reference counted, however currently layers free them
# when the layer is freed.

require 'test/unit'
require 'gdal/ogr'
require 'ogrtest'

class TestMemoryLayer < Test::Unit::TestCase
  def open_ds
    # Get the pat to the file
    file_name = File.join(data_directory, 'poly.shp')
    
    # Open datasource in update mode
    return Gdal::Ogr.open(file_name, 1)
  end   

  
  def test_layer_owns_feature
    ds = open_ds
    layer = ds.get_layer(0)

    feature = nil
    layer.each do |feat|
      feature = feat
      break
    end

    # free ds and layer
    ds = nil
    layer = nil

    # Now access the fieldefn
    field_defn = feature.get_defn_ref
    assert_equal("poly", field_defn.get_name, "Invalid field name")
  end

  def test_feature_owns_geoemtry
    ds = open_ds
    layer = ds.get_layer(0)

    feature = nil
    layer.each do |feat|
      feature = feat
      break
    end

    # get geometry
    geom = feature.get_geometry_ref()

    # Now access the geometry
    expected_value = "POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,479647.0 4765369.5,479730.375 4765400.5,480039.03125 4765539.5,480035.34375 4765558.5,480159.78125 4765610.5,480202.28125 4765482.0,480365.0 4765015.5,480389.6875 4764950.0,480133.96875 4764856.5,480080.28125 4764979.5,480082.96875 4765049.5,480088.8125 4765139.5,480059.90625 4765239.5,480019.71875 4765319.5,479980.21875 4765409.5,479909.875 4765370.0,479859.875 4765270.0,479819.84375 4765180.5))"
    assert_equal(expected_value, geom.export_to_wkt, "Invalid WKT value for geom")    
  end
end
