#!/usr/bin/env ruby

# Features reference count feature_defn, own geometries and
# own field values.

require 'test/unit'
require 'gdal/ogr'
require 'ogrtest'


class TestMemoryFeature < Test::Unit::TestCase
  def open_ds
    # Get the pat to the file
    file_name = File.join(data_directory, 'poly.shp')
    
    # Open datasource in update mode
    return Gdal::Ogr.open(file_name, 1)
  end   

  def test_owns_geoemtry
    ds = open_ds
    layer = ds.get_layer(0)

    feature = nil
    layer.each do |feat|
      feature = feat
      break
    end

    # get geometry
    geom = feature.get_geometry_ref()

    # free ds, layer and feature
    ds = nil
    layer = nil
    feature = nil

    # Now access the geometry
    expected_value = "POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,479647.0 4765369.5,479730.375 4765400.5,480039.03125 4765539.5,480035.34375 4765558.5,480159.78125 4765610.5,480202.28125 4765482.0,480365.0 4765015.5,480389.6875 4764950.0,480133.96875 4764856.5,480080.28125 4764979.5,480082.96875 4765049.5,480088.8125 4765139.5,480059.90625 4765239.5,480019.71875 4765319.5,479980.21875 4765409.5,479909.875 4765370.0,479859.875 4765270.0,479819.84375 4765180.5))"
    assert_equal(expected_value, geom.export_to_wkt, "Invalid WKT value for geom")    
  end

  def test_set_geometry
    ds = open_ds
    layer = ds.get_layer(0)

    feat = Gdal::Ogr::Feature.new( feature_def = layer.get_layer_defn())
    wkt = "POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,479647.0 4765369.5,479730.375 4765400.5,480039.03125 4765539.5,480035.34375 4765558.5,480159.78125 4765610.5,480202.28125 4765482.0,480365.0 4765015.5,480389.6875 4764950.0,480133.96875 4764856.5,480080.28125 4764979.5,480082.96875 4765049.5,480088.8125 4765139.5,480059.90625 4765239.5,480019.71875 4765319.5,479980.21875 4765409.5,479909.875 4765370.0,479859.875 4765270.0,479819.84375 4765180.5))"
    geom = Gdal::Ogr.create_geometry_from_wkt(wkt)

    feat.set_geometry(geom)
    feat = nil

    assert_equal(wkt, geom.export_to_wkt, "Invalid WKT value for geom")   
  end

  def test_set_geometry_directly
    ds = open_ds
    layer = ds.get_layer(0)

    feat = Gdal::Ogr::Feature.new( feature_def = layer.get_layer_defn())
    wkt = "POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,479647.0 4765369.5,479730.375 4765400.5,480039.03125 4765539.5,480035.34375 4765558.5,480159.78125 4765610.5,480202.28125 4765482.0,480365.0 4765015.5,480389.6875 4764950.0,480133.96875 4764856.5,480080.28125 4764979.5,480082.96875 4765049.5,480088.8125 4765139.5,480059.90625 4765239.5,480019.71875 4765319.5,479980.21875 4765409.5,479909.875 4765370.0,479859.875 4765270.0,479819.84375 4765180.5))"
    geom = Gdal::Ogr.create_geometry_from_wkt(wkt)

    feat.set_geometry_directly(geom)
    feat = nil

    assert_equal(wkt, geom.export_to_wkt, "Invalid WKT value for geom")   
  end
end
