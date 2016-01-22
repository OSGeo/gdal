#!/usr/bin/env ruby

require 'test/unit'
require 'gdal/osr'
require 'gdal/ogr'

class TestOsrCt < Test::Unit::TestCase
  def setup
    # Verify that we have PROJ.4 available. 
    utm_srs = Gdal::Osr::SpatialReference.new()
    utm_srs.set_utm(11)
    utm_srs.set_well_known_geog_cs('WGS84')

    ll_srs = Gdal::Osr::SpatialReference.new()
    ll_srs.set_well_known_geog_cs( 'WGS84' )

    begin
      ct = Gdal::Osr::CoordinateTransformation.new(ll_srs, utm_srs)
    rescue => e
      @have_proj4 = false
      puts e
    end
    
    @have_proj4 = true
  end

  def test_ll_to_utm()
    return if not @have_proj4

    utm_srs = Gdal::Osr::SpatialReference.new()
    utm_srs.set_utm(11)
    utm_srs.set_well_known_geog_cs('WGS84')

    ll_srs = Gdal::Osr::SpatialReference.new()
    ll_srs.set_well_known_geog_cs('WGS84')

    ct = Gdal::Osr::CoordinateTransformation.new(ll_srs, utm_srs)

    result = ct.transform_point(-117.5, 32.0, 0.0)
    assert_in_delta(result[0], 452772.06, 0.01)
    assert_in_delta(result[1], 3540544.89, 0.01)
    assert_in_delta(result[2], 0.0, 0.01)
  end

  
  # Transform an OGR geometry ... this is mostly aimed at ensuring that
  # the OGRCoordinateTransformation target SRS isn't deleted till the output
  # geometry which also uses it is deleted.
  def test_ogr_transform()
    return if not @have_proj4

    utm_srs = Gdal::Osr::SpatialReference.new()
    utm_srs.set_utm(11)
    utm_srs.set_well_known_geog_cs('WGS84')

    ll_srs = Gdal::Osr::SpatialReference.new()
    ll_srs.set_well_known_geog_cs('WGS84')

    ct = Gdal::Osr::CoordinateTransformation.new(ll_srs, utm_srs)

    pnt = Gdal::Ogr.create_geometry_from_wkt('POINT(-117.5 32.0)', ll_srs)
    result = pnt.transform(ct)

    out_srs = pnt.get_spatial_reference().export_to_pretty_wkt()

    assert_equal(out_srs[0..5], 'PROJCS', 
                 'output srs corrupt, ref counting issue?')
  end    
end
